"""Device authorization orchestration, shared by the CLI and the HTTP API.

One place for the sequence that both `flow` and `renew` (and the HTTP
`/api/authorize`) must perform:

    issue_challenge -> device signs -> verify_proof -> issue_license -> push

If the device rejects the freshly issued license, that license is revoked
immediately: otherwise the server would hold a "valid" license for a device
that never received it.
"""

from __future__ import annotations

import contextlib

from . import proto
from .service import AuthError


class FlowError(RuntimeError):
    """A step of the authorization sequence failed."""

    def __init__(self, message: str, code: str = "flow", detail: str = ""):
        super().__init__(message)
        self.code = code
        self.detail = detail


def authorize(svc, client, sid: str, dev: str, ttl_s: int = 900,
              perms: int = proto.PERM_FPGA, lock=None) -> dict:
    """Run the whole chain; returns {token, fields, ack, challenge, signature_hex}.

    ``lock`` (a context manager) serializes the *service/store* calls when the
    caller is multi-threaded. It is deliberately NOT held across the device
    round trips — those can take seconds and must not block other requests.
    """
    guard = lock if lock is not None else contextlib.nullcontext()

    try:
        with guard:
            challenge = svc.issue_challenge(sid, dev)
    except AuthError as exc:
        raise FlowError(str(exc), exc.code) from None

    ack = client.auth_start(challenge)
    if ack.get("result") != "ok":
        raise FlowError("device did not sign the challenge",
                        "device_proof_failed", str(ack.get("error", "")))
    data = ack.get("data", {}) or {}
    signature_hex = str(data.get("signature_hex", ""))

    try:
        with guard:
            svc.verify_proof(sid, dev, challenge, signature_hex,
                             data.get("pubkey_hex"), data.get("self_verify", 0))
    except AuthError as exc:
        raise FlowError(str(exc), exc.code) from None

    try:
        with guard:
            token = svc.issue_license(sid, dev, perms=perms, ttl_s=ttl_s)
    except AuthError as exc:
        raise FlowError(str(exc), exc.code) from None
    payload, _sig = proto.split_token(token)
    fields = proto.parse_payload(payload)

    ack = client.auth_verify(token, dev)
    if ack.get("result") != "ok":
        with guard:
            svc.revoke(fields["jti"])   # device never got it; do not leave it valid
        raise FlowError("device rejected the license", "push_failed",
                        str(ack.get("error", "")))

    return {"token": token, "fields": fields,
            "ack": ack.get("data", {}) or {},
            "challenge": challenge,
            "signature_hex": signature_hex}
