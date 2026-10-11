"""Decisive test: is the chip's CRC inside a known CRC family (for some init/xorout)?

Trick: within one CRC family, changing init and xorout shifts the result by a
constant that is *the same for every message*.  Therefore, for two observed
frames A and B, the difference

    CRC_family(A) XOR CRC_family(B)   must equal   chipCRC(A) XOR chipCRC(B)

independently of init/xorout.  One comparison settles the whole
(init, xorout) space for a given poly / reflection / coverage / byte order.

Observed frames (chip trailing bytes, lo first):
    A = [04 11] -> 33 43   => chip CRC = 0x4333
    B = [04 FF] -> 01 42   => chip CRC = 0x4201
Also tested: coverages with padding, reversed byte order, other polys.
ASCII output only.
"""

TARGET_XOR = 0x4333 ^ 0x4201        # = 0x0132 for the standard coverage

POLYS_REF = [0xA001, 0x8408, 0xA6BC, 0x91A0, 0xC002, 0x8005]
POLYS_NORM = [0x8005, 0x1021, 0x3D65, 0x0589]

FRAME_A = [0x04, 0x11]
FRAME_B = [0x04, 0xFF]


def crc_ref(data, poly, init=0):
    c = init
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c >> 1) ^ poly) if (c & 1) else (c >> 1)
    return c & 0xFFFF


def crc_norm(data, poly, init=0):
    c = init
    for b in data:
        c ^= (b << 8)
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFF if (c & 0x8000) else ((c << 1) & 0xFFFF)
    return c & 0xFFFF


COVERS = {
    "[04,11] vs [04,FF]        ": (FRAME_A, FRAME_B),
    "[11,04] vs [FF,04]        ": (list(reversed(FRAME_A)), list(reversed(FRAME_B))),
    "[04,11,0] vs [04,FF,0]    ": (FRAME_A + [0], FRAME_B + [0]),
    "[04,11,0,0] vs [04,FF,0,0]": (FRAME_A + [0, 0], FRAME_B + [0, 0]),
    "[11] vs [FF]              ": ([0x11], [0xFF]),
    "[04] vs [04]              ": ([0x04], [0x04]),
    "[04,11,33,43] vs [04,FF,01,42]": ([0x04, 0x11, 0x33, 0x43],
                                       [0x04, 0xFF, 0x01, 0x42]),
}

print("chip CRC(A) = 0x4333, chip CRC(B) = 0x4201 -> target XOR = 0x%04X" % TARGET_XOR)
print()
found = []
for cname, (a, b) in COVERS.items():
    for poly in POLYS_REF:
        d = crc_ref(a, poly) ^ crc_ref(b, poly)
        if d == TARGET_XOR:
            found.append(("reflected", hex(poly), cname))
    for poly in POLYS_NORM:
        d = crc_norm(a, poly) ^ crc_norm(b, poly)
        if d == TARGET_XOR:
            found.append(("normal", hex(poly), cname))
    print("  %s done" % cname)

print()
if found:
    print("*** FAMILY MATCH ***  the chip's CRC *is* inside these families:")
    for f in found:
        print("   ", f)
    print("=> solve init/xorout for these and adopt them (OUR side fixable).")
else:
    print("NO FAMILY MATCH: for none of %d poly x %d coverage combos does the"
          % (len(POLYS_REF) + len(POLYS_NORM), len(COVERS)))
    print("difference match, so no (init, xorout) can satisfy both frames.")
    print("=> the chip's CRC is NOT any standard CRC family => frame generation")
    print("   is non-conforming.")
