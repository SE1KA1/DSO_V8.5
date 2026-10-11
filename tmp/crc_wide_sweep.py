"""Wide CRC sweep: find ANY common CRC variant that yields the chip's trailing bytes.

Targets:
  bare/wake frame  [04 11] -> chip bytes 33 43   (i.e. CRC = 0x4333 if lo-first)
  command frame    [04 FF] -> chip bytes 01 42   (i.e. CRC = 0x4201)
ASCII output only.
"""

POLYS_NORM = [0x8005, 0x1021, 0x3D65, 0x0589]
POLYS_REF = [0xA001, 0x8408, 0xA6BC, 0x91A0]


def crc_ref(data, init, poly, refin=True, refout=True, xorout=0):
    c = init
    for b in data:
        if refin:
            b = int('{:08b}'.format(b)[::-1], 2)
        c ^= b
        for _ in range(8):
            c = ((c >> 1) ^ poly) if (c & 1) else (c >> 1)
    if refout != refin:
        c = int('{:016b}'.format(c)[::-1], 2)
    return (c ^ xorout) & 0xFFFF


def crc_norm(data, init, poly, xorout=0):
    c = init
    for b in data:
        c ^= (b << 8)
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFF if (c & 0x8000) else ((c << 1) & 0xFFFF)
    return (c ^ xorout) & 0xFFFF


CASES = [
    ("wake [04,11] -> 33 43", [0x04, 0x11], 0x4333),
    ("cmd  [04,FF] -> 01 42", [0x04, 0xFF], 0x4201),
]
COVER = {
    "[04,11]": [0x04, 0x11], "[11,04]": [0x11, 0x04], "[11]": [0x11], "[04]": [0x04],
    "[04,FF]": [0x04, 0xFF], "[FF,04]": [0xFF, 0x04], "[FF]": [0xFF],
}
for tag, base, target in CASES:
    print("== %s ==" % tag)
    hits = []
    for name, cov in COVER.items():
        for poly in POLYS_REF:
            for init in (0x0000, 0xFFFF):
                for xorout in (0x0000, 0xFFFF):
                    if crc_ref(cov, init, poly, True, True, xorout) == target:
                        hits.append(("ref", name, hex(poly), hex(init), hex(xorout)))
                    if crc_norm(cov, init, poly, xorout) == target:
                        hits.append(("norm", name, hex(poly), hex(init), hex(xorout)))
    if hits:
        for h in hits:
            print("   HIT:", h)
    else:
        print("   no hit among", len(COVER) * (len(POLYS_REF) * 2 * 2 + len(POLYS_NORM) * 2 * 2),
              "convention/coverage combinations")
