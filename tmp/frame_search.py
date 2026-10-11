"""Search the captured byte streams for ANY valid ATECC response frame.

ATECC response group = [Count][Data...(Count-3 bytes)][CRC_lo][CRC_hi]
CRC16 over [Count..Data] (Count-2 bytes).  Poly 0x8005 reflected (0xA001), init 0, LSB first.

If the device's stream is parsed with a wrong frame boundary, a *different*
offset/length may still validate -> which would mean OUR framing is off (software bug),
not the chip.

ASCII output only.
"""


def crc_ref(data, init=0):
    c = init
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c >> 1) ^ 0xA001) if (c & 1) else (c >> 1)
    return c & 0xFFFF


def crc_norm(data, init=0):
    c = init
    for b in data:
        c ^= (b << 8)
        for _ in range(8):
            c = ((c << 1) ^ 0x8005) & 0xFFFF if (c & 0x8000) else ((c << 1) & 0xFFFF)
    return c


STREAMS = {
    "post-command (10B read)": [0x04, 0xFF, 0x01, 0x42, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF],
    "bare-read#2/#3 (6B read)": [0x04, 0x11, 0x33, 0x43, 0xFF, 0xFF],
    "bare-read#1 (6B read)": [0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF],
}

print("Frame search: any offset/length whose trailing 2 bytes match the CRC of the preceding bytes")
for tag, s in STREAMS.items():
    print("--- %s : %s" % (tag, " ".join("%02X" % x for x in s)))
    hits = []
    n = len(s)
    for off in range(n):
        for cnt in range(5, n - off + 1):          # Count value 5..len
            # group occupies s[off : off+cnt]; CRC over first cnt-2 bytes
            body = s[off:off + cnt - 2]
            if len(body) < 3:
                continue
            for name, fn in (("ref0", crc_ref), ("norm0", crc_norm)):
                c = fn(body)
                lo, hi = s[off + cnt - 2], s[off + cnt - 1]
                if (c & 0xFF) == lo and (c >> 8) == hi:
                    hits.append((off, cnt, name, hex(c)))
    print("    hits:", hits if hits else "NONE (no offset/length validates under ref/norm, init 0)")
print()
print("Also: is the trailing CRC maybe computed INCLUDING the count only (1 data byte)?")
for tag, s in STREAMS.items():
    if len(s) >= 4:
        c = crc_ref([s[0], s[1]])
        print("  %-26s CRC({%02X,%02X}) = 0x%04X  chip bytes = %02X %02X" %
              (tag, s[0], s[1], c, s[2], s[3]))
