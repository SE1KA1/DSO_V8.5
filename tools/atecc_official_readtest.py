#!/usr/bin/env python3
"""ATECC608B independent verification using Microchip's official CryptoAuthLib.

Purpose: settle "is it our firmware or the chip?" with a second, independent
implementation.  The official I2C HAL performs the spec-compliant wake itself
(SDA low >= 60us -> release -> wait >= 1500us) before every command, so a
negative result here cannot be blamed on wake-up handling.

Typical use (Raspberry Pi / any Linux SBC with the chip wired to its I2C bus):

  1) enable I2C and install tools + library
       sudo raspi-config          # Interface Options -> I2C -> enable
       sudo apt install -y i2c-tools python3-pip
       pip3 install cryptoauthlib
       i2cdetect -y 1             # NOTE: a sleeping ATECC is invisible to a
                                  # plain scan, so "nothing seen" is expected
  2) run this script
       python3 atecc_official_readtest.py

Reading the result:
  * Revision (e.g. 00 00 60 03) printed and config zone dumped
      -> CHIP IS FINE.  Our firmware/protocol layer is at fault.
         Please send the output back so we can fix our side.
  * Errors / no response
      -> the official library also cannot talk to it => the part is not a
         conforming ATECC (send this output to the supplier).

If you get AttributeError on the config helper, the Python API differs between
versions; run  pip3 show cryptoauthlib  and report the version.

Output is ASCII only (safe for any console codepage).
"""
import sys


def main():
    try:
        import cryptoauthlib as cal
    except Exception as exc:                                  # noqa: BLE001
        print("IMPORT cryptoauthlib failed:", exc)
        print("hint: pip3 install cryptoauthlib  (and enable the I2C bus)")
        return 2

    print("cryptoauthlib version:", getattr(cal, "__version__", "unknown"))

    # --- pick the config helper that exists in this version -----------------
    cfg = None
    try:
        cfg = cal.cfg_ateccx08a_i2c_default()
    except Exception:                                         # noqa: BLE001
        try:
            cfg = cal.atcab_get_cfg()
        except Exception as exc:                              # noqa: BLE001
            print("cannot build a config object:", exc)
            print("send this output back and we will adjust for your version")
            return 3

    # --- point it at /dev/i2c-1, 7-bit address 0x60 -------------------------
    try:
        cfg.cfg.atcai2c.bus = 1
        cfg.cfg.atcai2c.address = 0x60
    except Exception as exc:                                  # noqa: BLE001
        print("setting bus/address failed:", exc)
        return 4

    rc = cal.atcab_init(cfg)
    print("atcab_init rc =", rc, "(0 = success)")
    if rc != 0:
        print("VERDICT: init failed -> chip does not answer the official library")
        return 1

    # --- Info(Revision) ----------------------------------------------------
    info = bytearray(4)
    rc_info = cal.atcab_info(info)
    print("atcab_info rc =", rc_info, "(0 = success)")
    if rc_info == 0:
        print("Revision =", " ".join("%02X" % b for b in info))
    else:
        print("Info failed -> no valid response to the official library")

    # --- Read Config zone (128 bytes) --------------------------------------
    rc_cfg = -1
    try:
        buf = bytearray(128)
        rc_cfg = cal.atcab_read_config_zone(buf)
        print("atcab_read_config_zone rc =", rc_cfg, "(0 = success)")
        if rc_cfg == 0:
            for blk in range(4):
                print("cfg%02X: %s" % (
                    blk * 32,
                    " ".join("%02X" % b for b in buf[blk * 32:(blk + 1) * 32])))
    except Exception as exc:                                  # noqa: BLE001
        print("read_config_zone raised:", exc)

    print()
    if rc_info == 0 or rc_cfg == 0:
        print("VERDICT: CHIP OK -> our implementation is at fault "
              "(please send this output back)")
    else:
        print("VERDICT: CHIP DOES NOT RESPOND to the official library "
              "-> part is non-conforming (supplier evidence)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
