"""Verify OUR CRC16 implementation against the published CRC-16/ARC check value.

ATECC uses CRC-16 with poly 0x8005, reflected (0xA001), init 0x0000, LSB-first
== the standard parameter set of CRC-16/ARC.
Reference check value: CRC-16/ARC("123456789") = 0xBB3D (published catalogue value).

If our implementation prints 0xBB3D, our CRC is exactly the standard CRC-16/ARC,
i.e. the algorithm itself cannot be the reason the chip's frames fail to verify.

ASCII output only.
"""


def crc_ours(data, init=0x0000):          # same code as firmware atecc_crc16()
    crc = init
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xA001) if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def crc_ours_initffff(data):              # same but init 0xFFFF (for comparison)
    return crc_ours(data, 0xFFFF)


check = b"123456789"
print("CRC-16/ARC check vector: CRC(\"123456789\") should be 0xBB3D")
print("  ours (init 0x0000) = 0x%04X  -> %s" %
      (crc_ours(check), "MATCH: our algorithm IS standard CRC-16/ARC"
       if crc_ours(check) == 0xBB3D else "MISMATCH!"))
print("  ours (init 0xFFFF) = 0x%04X  (CRC-16/MAXIM-DOW check value is 0x44C2)" %
      crc_ours_initffff(check))
print()
print("Frames actually read from the chip (init 0x0000 vs 0xFFFF):")
for tag, frame, lo, hi in [("bare 04 11 33 43", [0x04, 0x11], 0x33, 0x43),
                           ("post 04 FF 01 42", [0x04, 0xFF], 0x01, 0x42)]:
    c0 = crc_ours(frame)
    cf = crc_ours_initffff(frame)
    print("  %-18s chip_crc=%02X %02X | ours(init0)=%04X ours(initFFFF)=%04X" %
          (tag, lo, hi, c0, cf))
