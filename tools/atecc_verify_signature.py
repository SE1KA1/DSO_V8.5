#!/usr/bin/env python3
"""用 PC 端独立验签，验证 ATECC 的 Sign 结果（Stage 2 Slot0 预置后的端到端验证）。

为什么需要它
------------
ATECC 的 `Sign`（0x41 Mode 0x80）返回的是 **64 字节 r||s**（原始大端整数），不是 DER；
而且我们走的是 `Nonce(0x16 Mode 0x03)` 直通模式 —— **32 字节挑战被直接放进 TempKey 当作消息摘要**，
所以验签时 **digest 就是挑战原文，不要再哈希一次**（这是最容易搞错的一点）。

用法
----
  1) 打印固件里那把"一次性测试密钥"的公钥（用于核对 C 常量）：
        python tools/atecc_verify_signature.py --devkey
  2) 自检（本地签一次再验一次，确认脚本本身可用）：
        python tools/atecc_verify_signature.py --selftest
  3) 验证设备返回的签名（`atecc.auth_start` 的 data.signature_hex / data.pubkey_hex）：
        python tools/atecc_verify_signature.py \
            --pubkey   <128 hex>  --challenge <64 hex>  --signature <128 hex>

安全说明
--------
本脚本只做本地数学运算，不碰硬件；内置的"开发测试密钥"是**公开的一次性向量**（私钥 = 00 01 .. 1F），
仅用于打通链路，**不可用于任何正式用途**。
"""
import argparse
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils

# 与固件 dso_atecc608b.c 里 k_dev_priv / k_dev_pub 完全一致（改动请同步）
DEV_PRIV = bytes(range(0, 32))          # 00 01 02 ... 1F（32 字节，与固件 k_dev_priv 一致）


def _pub_from_priv(priv: bytes, curve=ec.SECP256R1()) -> bytes:
    d = int.from_bytes(priv, "big")
    n = ec.derive_private_key(d, curve).public_key().public_numbers()
    return n.x.to_bytes(32, "big") + n.y.to_bytes(32, "big")


def _verify_raw(pub_hex: str, digest_hex: str, sig_hex: str) -> bool:
    """pubkey = X||Y(64B)；digest = 32B（挑战原文）；sig = r||s(64B)。"""
    pub = bytes.fromhex(pub_hex)
    digest = bytes.fromhex(digest_hex)
    sig = bytes.fromhex(sig_hex)
    if len(pub) != 64:
        raise ValueError("pubkey 必须是 64 字节（X||Y）")
    if len(digest) != 32:
        raise ValueError("challenge 必须是 32 字节")
    if len(sig) != 64:
        raise ValueError("signature 必须是 64 字节（r||s）")
    pn = ec.EllipticCurvePublicNumbers(
        int.from_bytes(pub[:32], "big"), int.from_bytes(pub[32:], "big"),
        ec.SECP256R1())
    pk = pn.public_key()
    der = utils.encode_dss_signature(
        int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big"))
    try:
        # Prehashed：digest 已是最终摘要（Nonce Mode 0x03 让挑战直接充当摘要）
        pk.verify(der, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        return True
    except InvalidSignature:
        return False


def main() -> int:
    ap = argparse.ArgumentParser(description="ATECC Sign 结果的 PC 端验签")
    ap.add_argument("--devkey", action="store_true",
                    help="打印固件内置测试密钥的公钥（核对 C 常量用）")
    ap.add_argument("--selftest", action="store_true", help="本地自检")
    ap.add_argument("--pubkey", help="64 字节公钥 X||Y（hex）")
    ap.add_argument("--challenge", help="32 字节挑战（hex）")
    ap.add_argument("--signature", help="64 字节签名 r||s（hex）")
    a = ap.parse_args()

    dev_pub = _pub_from_priv(DEV_PRIV)

    if a.devkey:
        print("dev priv (hex):", DEV_PRIV.hex())
        print("dev pub  (hex):", dev_pub.hex())
        print("固件里 k_dev_pub[64] 应与之逐字节一致。")
        return 0

    if a.selftest:
        d = int.from_bytes(DEV_PRIV, "big")
        sk = ec.derive_private_key(d, ec.SECP256R1())
        digest = bytes(range(32))
        der = sk.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        r, s = utils.decode_dss_signature(der)
        raw = r.to_bytes(32, "big") + s.to_bytes(32, "big")
        ok = _verify_raw(dev_pub.hex(), digest.hex(), raw.hex())
        print("selftest:", "OK" if ok else "FAILED")
        # 反向：篡改摘要应验签失败
        bad = _verify_raw(dev_pub.hex(), bytes(range(32, 64)).hex(), raw.hex())
        print("selftest (tampered digest, expect False):", bad)
        return 0 if (ok and not bad) else 1

    if not (a.pubkey and a.challenge and a.signature):
        ap.print_help()
        print("\n提示：先用 --devkey / --selftest 检查工具本身可用。")
        return 2

    ok = _verify_raw(a.pubkey, a.challenge, a.signature)
    print("VERDICT:", "SIGNATURE VALID" if ok else "SIGNATURE INVALID")
    if not ok:
        print("排查顺序：① challenge 是否就是设备收到的那 32 字节（不要再哈希）")
        print("          ② pubkey 是否是设备读回的那 64 字节（X||Y 大端）")
        print("          ③ 签名是否 64 字节 r||s（不是 DER）")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
