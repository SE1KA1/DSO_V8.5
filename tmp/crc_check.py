"""ATECC response-frame CRC check (ASCII output only).

Checks whether the frames we read are valid ATECC frames:
  - bare-read frame (no command sent) : 04 11 33 43
  - post-command frame                : 04 FF 01 42
CRC16: poly 0x8005 reflected (0xA001), init 0, LSB-first (lo byte then hi byte).
"""
import itertools


def crc_ref(data, init=0, poly=0xA001):
    c = init
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c >> 1) ^ poly) if (c & 1) else (c >> 1)
    return c & 0xFFFF


def crc_norm(data, init=0, poly=0x8005):
    c = init
    for b in data:
        c ^= (b << 8)
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFF if (c & 0x8000) else ((c << 1) & 0xFFFF)
    return c


def report(tag, frame, exp_lo, exp_hi):
    print("%s  frame=%s  chip_crc_bytes=lo=%02X hi=%02X" %
          (tag, " ".join("%02X" % x for x in frame), exp_lo, exp_hi))
    c = crc_ref(frame)
    ok = (c & 0xFF) == exp_lo and (c >> 8) == exp_hi
    print("   CRC16(ref,init0) over whole frame = 0x%04X  -> %s" %
          (c, "VALID" if ok else "mismatch"))
    # also try: CRC over frame[:-2] (count+data), which is the ATECC convention
    c2 = crc_ref(frame[:-2])
    ok2 = (c2 & 0xFF) == exp_lo and (c2 >> 8) == exp_hi
    print("   CRC16(ref,init0) over count+data  = 0x%04X  -> %s" %
          (c2, "VALID" if ok2 else "mismatch"))
    return ok2


print("== bare-read frame (NO command sent) ==")
report("A", [0x04, 0x11, 0x33, 0x43], 0x33, 0x43)
print()
print("== post-command frame ==")
report("B", [0x04, 0xFF, 0x01, 0x42], 0x01, 0x42)
print()

print("== brute force: which (count, status) pairs give CRC = 0x4333 ==")
hits = []
for cnt in range(256):
    for st in range(256):
        if crc_ref([cnt, st]) == 0x4333:
            hits.append((cnt, st))
print("   count=0x04 candidates:", ["(04,%02X)" % s for c, s in hits if c == 0x04] or "none")
print("   total pairs:", len(hits))

print()
print("== brute force: which (count, status) pairs give CRC = 0x4201 ==")
hits2 = []
for cnt in range(256):
    for st in range(256):
        if crc_ref([cnt, st]) == 0x4201:
            hits2.append((cnt, st))
print("   count=0x04 candidates:", ["(04,%02X)" % s for c, s in hits2 if c == 0x04] or "none")
print("   total pairs:", len(hits2))
