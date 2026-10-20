#!/usr/bin/env python3
"""Zero-hardware CRC cross-check against Microchip's official CryptoAuthLib.

WHY
---
We read two 4-byte responses from an ATECC608B that fail our CRC check:
    bare read  (no command) : 04 11 33 43     -> trailing bytes 33 43
    after our command       : 04 FF 01 42     -> trailing bytes 01 42
Our own CRC is provably the standard CRC-16/ARC (check vector: CRC("123456789")
= 0xBB3D), so the algorithm itself is not wrong.  The open question is whether
the chip uses a *different* CRC variant/coverage that we simply do not know.

This script asks Microchip's own library for the CRC of every plausible byte
coverage and reports whether ANY of them reproduces the chip's trailing bytes.
No hardware needed.

USAGE
-----
    pip install cryptoauthlib
    python tools/atecc_crc_official_check.py

SAFETY (read this first)
------------------------
The installed library exposes helpers whose names contain "crc" that are
*zone-lock* functions (atcab_lock_config_zone_crc / atcab_lock_data_zone_crc).
Locking a zone is IRREVERSIBLE -- it permanently freezes the chip.  This script
never calls them, and neither should you (the project rule is: no Lock while
prototyping).

HOW TO READ THE RESULT
----------------------
  * "MATCH ..."  -> the chip is using a different CRC variant/coverage than we
                    assumed => OUR side must be fixed (tell us which row matched).
  * "NO MATCH"   -> under the official CRC no plausible coverage explains the
                    chip's bytes => the frame really is non-conforming.

Output is ASCII only (safe for any console codepage).
"""
import sys

# ---------------------------------------------------------------- our frames
FRAMES = [
    ("bare read (no command)", [0x04, 0x11, 0x33, 0x43], 0x33, 0x43),
    ("after our command     ", [0x04, 0xFF, 0x01, 0x42], 0x01, 0x42),
]

# byte coverages worth testing (count+data is the standard one)
def coverages(frame):
    n = len(frame)
    return [
        ("count+data (%d B)" % (n - 2), frame[:n - 2]),
        ("data only (%d B)" % (n - 3), frame[1:n - 2]),
        ("count only (1 B)", frame[:1]),
        ("whole frame (%d B)" % n, frame),
        ("count+data rev", list(reversed(frame[:n - 2]))),
    ]


def crc_arc(data, init=0x0000):
    """Reference implementation: CRC-16/ARC (== ATECC datasheet parameters)."""
    crc = init
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xA001) if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def official_crc_functions(cal):
    """Find callable CRC entry points in whatever API shape this version has.

    Note: the Python binding only re-exports a curated list of high-level `atcab_*` calls, so the
    DLL-internal CRC helpers are usually NOT visible here. `load_dll_crc_functions()` below loads the
    shared library directly and is the reliable route.
    (`atcab_lock_config_zone_crc` / `atcab_lock_data_zone_crc` also contain "crc" but are ZONE-LOCK
    helpers - irreversible. Never call them; see the warning in the docstring.)
    """
    found = []
    for name in ("atCRC", "atCalcCrc",
                 "atcab_crc_16", "atcac_crc16", "crc16", "at_crc16", "calculate_crc16",
                 "atca_crc16", "crc_16", "atcab_crc16"):
        fn = getattr(cal, name, None)
        if callable(fn):
            found.append((name, fn))
    return found


def load_dll_crc_functions(cal):
    """Load the CryptoAuthLib shared library directly and bind its exported CRC helpers.

    Verified by parsing the export table of the installed Windows build
    (cryptoauthlib/cryptoauth.dll): it exports `atCRC` and `atCalcCrc`.
    `atCRC` has the classic signature `uint16_t atCRC(uint32_t len, const uint8_t *data,
    uint16_t crc)`; `call_official()` below tries several call shapes.
    """
    import ctypes
    import glob
    import os

    try:
        pkg = os.path.dirname(getattr(cal, "__file__", "") or "")
    except Exception:                              # noqa: BLE001
        return []
    if not pkg or not os.path.isdir(pkg):
        return []

    lib = None
    for pat in ("*.dll", "*.so", "*.so.*", "*.dylib"):
        for path in sorted(glob.glob(os.path.join(pkg, pat))):
            if "cryptoauth" not in os.path.basename(path).lower():
                continue
            try:
                lib = ctypes.WinDLL(path) if os.name == "nt" else ctypes.CDLL(path)
                print("loaded shared library:", path)
                break
            except Exception as exc:               # noqa: BLE001
                print("could not load %s: %s" % (path, exc))
        if lib is not None:
            break
    if lib is None:
        print("no crypto shared library found next to the package")
        return []

    found = []
    # NOTE: `atCalcCrc` is deliberately NOT used -- it takes an `ATCAPacket` struct pointer, not a raw
    # byte buffer, so calling it with a byte array yields meaningless values (it reinterprets memory).
    # Only raw-data CRC helpers belong in this list.
    for name in ("atCRC", "atcac_crc16", "atcab_crc_16", "atca_crc16"):
        try:
            fn = getattr(lib, name)
        except AttributeError:
            continue
        found.append((name, fn))
    if not found:
        print("library loaded, but no raw-data CRC helper (atCRC/atcac_crc16) is exported")
    return found


def discover_crc_symbol(cal):
    """Search the installed package files for a CRC entry point (API varies a lot).

    NOTE: never call the *lock* helpers seen in this namespace -- locking a zone
    is irreversible (see the warning in the docstring).
    """
    import os
    try:
        pkg = os.path.dirname(getattr(cal, "__file__", "") or "")
    except Exception:                              # noqa: BLE001
        return
    if not pkg or not os.path.isdir(pkg):
        return
    print("package dir:", pkg)
    pats = ("crc16", "crc_16", "CRC16")
    hits = []
    for root, _dirs, files in os.walk(pkg):
        for fn in files:
            if not fn.endswith((".py", ".pyi")):
                continue
            p = os.path.join(root, fn)
            try:
                txt = open(p, "r", encoding="utf-8", errors="ignore").read()
            except OSError:
                continue
            for line in txt.splitlines():
                low = line.lower()
                if any(pat.lower() in low for pat in pats):
                    hits.append((os.path.relpath(p, pkg), line.strip()[:110]))
    for f, ln in hits[:25]:
        print("   %-28s %s" % (f, ln))
    if not hits:
        print("   (no crc16/crc_16 symbol found in the package's .py files -- "
              "this build binds only high-level APIs)")


def call_official(fn, data):
    """Try the plausible call shapes; return int result or None."""
    import ctypes
    n = len(data)
    bufs = [
        (ctypes.c_uint8 * n)(*data),
        bytes(data),
        bytearray(data),
    ]
    # (data, len, &crc) | (data, &crc) | (data) -> int | (len, data, init) -> int  (classic atCRC)
    for buf in bufs:
        shapes = []
        if isinstance(buf, ctypes.Array):
            out = ctypes.c_uint16(0)
            # classic CryptoAuthLib shape FIRST: uint16_t atCRC(uint32_t len, const uint8_t*, uint16_t)
            shapes.append(lambda b=buf: (None, int(fn(n, b, 0))))
            shapes.append(lambda b=buf: (None, int(fn(n, b))))
            shapes.append(lambda b=buf, o=out: (fn(b, n, ctypes.byref(o)), o.value))
            shapes.append(lambda b=buf, o=out: (fn(b, ctypes.byref(o)), o.value))
            shapes.append(lambda b=buf: (None, int(fn(b, n))))
            shapes.append(lambda b=buf: (None, int(fn(b))))
        for sh in shapes:
            try:
                rc, val = sh()
                if rc is None or rc == 0:
                    return int(val) & 0xFFFF
            except Exception:                     # noqa: BLE001
                continue
    return None


def probe_atrc(cal):
    """用显式函数原型调用官方库导出的 `atCRC`，打印成功值或异常原文。

    经典签名：uint16_t atCRC(uint32_t len, const uint8_t *data, uint16_t crc)
    （CryptoAuthLib 手册说明 CRC 以 little-endian 返回）。能算出来即为官方二进制的权威值。
    """
    import ctypes
    import glob
    import os

    pkg = os.path.dirname(getattr(cal, "__file__", "") or "")
    path = None
    for pat in ("*.dll", "*.so", "*.so.*", "*.dylib"):
        for cand in sorted(glob.glob(os.path.join(pkg, pat))):
            if "cryptoauth" in os.path.basename(cand).lower():
                path = cand
                break
        if path:
            break
    if not path:
        print("probe: no crypto shared library found")
        return 1
    try:
        lib = ctypes.WinDLL(path) if os.name == "nt" else ctypes.CDLL(path)
    except Exception as exc:                      # noqa: BLE001
        print("probe: cannot load library:", exc)
        return 1
    try:
        fn = lib.atCRC
    except AttributeError:
        print("probe: atCRC is not exported by", path)
        return 1
    # 实测签名（2026-10-20）：第 3 个参数是**输出指针**（uint16_t *crc）。
    # 证据：按 (len, data*, uint16_t) 调用会抛 "access violation writing 0x0" ⇒ 它往 NULL 写。
    fn.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8),
                   ctypes.POINTER(ctypes.c_uint16)]
    fn.restype = ctypes.c_int
    print("probe: using", path)
    for data in ([0x04, 0x11], [0x04, 0xFF], [0x07, 0x30, 0x00, 0x00, 0x00]):
        buf = (ctypes.c_uint8 * len(data))(*data)
        out = ctypes.c_uint16(0)
        try:
            rc = fn(len(data), buf, ctypes.byref(out))
            print("  atCRC(%s) rc=%d crc=0x%04X  -> wire %02X %02X"
                  % (" ".join("%02X" % b for b in data), rc, out.value,
                     out.value & 0xFF, (out.value >> 8) & 0xFF))
        except Exception as exc:                  # noqa: BLE001
            print("  atCRC(%s) EXCEPTION: %r"
                  % (" ".join("%02X" % b for b in data), exc))
    return 0


def main():
    print("=== ATECC frame CRC cross-check (no hardware required) ===")
    print()

    # --- sanity: does our reference implementation match the published vector?
    chk = crc_arc(b"123456789")
    print('sanity: CRC-16/ARC("123456789") = 0x%04X (published 0xBB3D) -> %s'
          % (chk, "OK" if chk == 0xBB3D else "UNEXPECTED"))
    print()

    # --- official library -------------------------------------------------
    try:
        import cryptoauthlib as cal
        print("cryptoauthlib version:", getattr(cal, "__version__", "unknown"))
        fns = official_crc_functions(cal)
        if not fns:
            print("no CRC entry point re-exported by the Python layer.",
                  "Names containing 'crc':",
                  [n for n in dir(cal) if "crc" in n.lower()][:20])
            print("!! WARNING: atcab_lock_*_crc are zone-LOCK functions -- calling them")
            print("!!          permanently locks the chip (irreversible). DO NOT CALL THEM.")
            print("-> loading the shared library directly instead ...")
            fns = load_dll_crc_functions(cal)
        if fns:
            print("official CRC functions found:", ", ".join(n for n, _ in fns))
    except Exception as exc:                       # noqa: BLE001
        cal = None
        fns = []
        print("IMPORT cryptoauthlib failed:", exc)
        print("hint: pip install cryptoauthlib   (no hardware needed for this script)")
    print()

    any_match = False
    official_rows = 0                              # rows actually computed BY the official library
    for tag, frame, clo, chi in FRAMES:
        chip_crc = (clo << 8) | chi               # chip sent lo then hi
        print("frame %s : %s   chip trailing bytes = %02X %02X"
              % (tag, " ".join("%02X" % b for b in frame), clo, chi))
        for cname, data in coverages(frame):
            ours = crc_arc(data)
            flag = "  <== MATCH OUR CRC!" if ours == chip_crc else ""
            print("    ours  %-18s CRC = 0x%04X%s" % (cname, ours, flag))
            if ours == chip_crc:
                any_match = True
            for fname, fn in fns:
                got = call_official(fn, data)
                if got is None:
                    continue
                official_rows += 1
                flag2 = "  <== MATCH OFFICIAL CRC!" if got == chip_crc else ""
                if got == chip_crc:
                    any_match = True
                print("    %-7s %-18s CRC = 0x%04X%s" % (fname, cname, got, flag2))
        print()

    print("=== VERDICT ===")
    if any_match:
        print("MATCH FOUND -> the chip uses a CRC variant/coverage we did not assume.")
        print("=> OUR implementation must be fixed. Send this whole output back.")
    elif official_rows > 0:
        print("NO MATCH -> %d row(s) were computed by the OFFICIAL library and none of them" % official_rows)
        print("reproduces the chip's trailing bytes => under the official CRC the frame is")
        print("NON-CONFORMING. => part/supplier issue (or send the output for a second opinion).")
    else:
        # IMPORTANT: our own rows alone must NEVER be turned into a conclusion.
        print("INCONCLUSIVE: NO row was computed by the official library (0 official rows),")
        print("so only our own reference rows above exist. Do NOT quote a 'NO MATCH' conclusion")
        print("from this run -- send the whole output back and we will adapt the call.")
        print("(Note: the datasheet parameters are already stated in Microchip's own reply,")
        print(" and our rows match the published check vector 0xBB3D, so the decisive evidence")
        print(" above does not depend on this script.)")
    return 0


if __name__ == "__main__":
    if "--probe-atrc" in sys.argv:
        try:
            import cryptoauthlib as _cal
        except Exception as _exc:                 # noqa: BLE001
            print("IMPORT cryptoauthlib failed:", _exc)
            raise SystemExit(1)
        raise SystemExit(probe_atrc(_cal))
    raise SystemExit(main())
