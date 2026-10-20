# Reply to Microchip Support — ATECC608B-SSHDA-T (follow-up)

**Subject:** ATECC608B-SSHDA-T — Info returns `00 00 60 03`, yet the device only accepts a **16-bit bit-reversed** CRC

Dear Microchip Support,

Thank you for the reply. We can provide the requested revision bytes — however, reading them already
exposes the problem we are reporting. Everything below is reproducible with the two 8-byte writes
shown in §1 and §6.

---

## 1. Answer to your question: the revision returned by the Info command

```
sent     : 03 07 30 00 00 00 03 5D        <- CRC = 16-bit bit-reversal of the specified CRC
received : 07 00 00 60 03 83
=> RevNum = 00 00 60 03                   <- exactly the value you state for a genuine ATECC608B
```

**The important part:** we can only obtain that response when the two CRC bytes are the
**bit-reversal** of the CRC. With the CRC computed exactly as you describe it, the **byte-identical
command** (same word address, same Count, same opcode, same parameters) is refused:

```
sent     : 03 07 30 00 00 00 BA C0        <- CRC exactly as specified (poly 0x8005, init 0, LSB first)
received : 04 FF 01 42                    <- 0xFF = "CRC or other communication error"
```

So: the device answers *with the correct ATECC608B revision* as soon as its CRC requirement is met —
the open question is only **which CRC it accepts**, and that is not the one in the datasheet.

---

## 2. Clarification: we implement precisely the CRC you describe

You wrote: *poly 0x8005, initialised to zero, CRC bytes transmitted LSB first.* That is exactly what
we implemented, and it matches the published standard check vector for those parameters:

```python
def crc16_arc(data):        # poly 0x8005 reflected (0xA001), init 0
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF

def rev16(v):               # 16-bit bit-reversal (the transform our device demands)
    r = 0
    for i in range(16):
        r = (r << 1) | ((v >> i) & 1)
    return r & 0xFFFF

assert crc16_arc(b"123456789") == 0xBB3D                       # published check vector
assert crc16_arc(bytes([0x07,0x30,0x00,0x00,0x00])) == 0xC0BA   # Info
assert rev16(0xC0BA) == 0x5D03                                 # what the device accepts instead
```

Therefore our observation is **not** "our CRC-16 differs from yours". We compute *your* CRC and the
device rejects it; the device accepts only its 16-bit reversal.

---

## 3. Controlled experiment: 5 different commands × 3 CRC variants

Each command was sent three times. **Only the last two bytes (the CRC) differ**; every other byte is
identical. Read-only commands only (Info 0x30, Random 0x1B, Read 0x02 in two modes, Nonce 0x16 with
32 data bytes — a 40-byte frame whose CRC spans 37 bytes).

| variant | accepted |
|---|---|
| CRC as specified (LSB first) | **0 / 5** |
| **16-bit bit-reversal of the CRC** | **5 / 5** |
| two CRC **bytes swapped** (MSB first) | **0 / 5** |

Raw log (excerpt):

```
-- #1 Info   rev    0x30 P1=00  count=0x07 frame=8 bytes  stdCRC=0xC0BA
     std   0xC0BA -> tail BA C0 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
     rev16 0x5D03 -> tail 03 5D : resp=07 00 00 60  [ACCEPTED]
     swap  0xBAC0 -> tail C0 BA : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
-- #2 Random        0x1B P1=00  count=0x07 frame=8 bytes  stdCRC=0x24B3
     std   0x24B3 -> tail B3 24 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
     rev16 0xCD24 -> tail 24 CD : resp=23 FF FF 00  [ACCEPTED]
     swap  0xB324 -> tail 24 B3 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
-- #3 Read  32B cfg 0x02 P1=80  count=0x07 frame=8 bytes  stdCRC=0x90B5
     std   0x90B5 -> tail B5 90 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
     rev16 0xAD09 -> tail 09 AD : resp=23 01 23 29  [ACCEPTED]
     swap  0xB590 -> tail 90 B5 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
-- #4 Read   4B cfg 0x02 P1=00  count=0x07 frame=8 bytes  stdCRC=0x78B4
     std   0x78B4 -> tail B4 78 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
     rev16 0x2D1E -> tail 1E 2D : resp=07 01 23 29  [ACCEPTED]
     swap  0xB478 -> tail 78 B4 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
-- #5 Nonce 32B     0x16 P1=03  count=0x27 frame=40 bytes  stdCRC=0xC0B0
     std   0xC0B0 -> tail B0 C0 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
     rev16 0x0D03 -> tail 03 0D : resp=04 00 03 40  [ACCEPTED]
     swap  0xB0C0 -> tail C0 B0 : resp=04 FF 01 42  [CRC REJECTED (st=0xFF)]
VERDICT: std 0/5 accepted | rev16 5/5 accepted | swap 0/5 accepted
```

The accepted frames were **executed correctly** — i.e. the device really did parse them:

| # | response | meaning |
|---|---|---|
| 1 | `07 00 00 60 03` | Count=7 + RevNum `00 00 60 03` |
| 2 | `23 FF FF 00 …` | Count=0x23=35 ⇒ 32 random bytes (Random executed) |
| 3 | `23 01 23 29 …` | Count=0x23 + config-zone bytes `01 23 29 16 …` (Read executed) |
| 4 | `07 01 23 29` | 4-byte read returns the same bytes (consistent with #3) |
| 5 | `04 00 03 40` | **status 0x00 = success** (Nonce executed) + CRC bytes `03 40` = `rev16(CRC of {04,00})` |

The device's own spontaneous status frames follow the same rule:

| frame content | Std CRC | `rev16` | bytes actually sent |
|---|---|---|---|
| `04 11` (after wake, before first command) | 0xCCC2 | 0x4333 | `33 43` |
| `04 FF` (CRC error) | 0x8042 | 0x4201 | `01 42` |
| `04 00` (Nonce success) | 0xC002 | 0x4003 | `03 40` |

All of the above CRC values were recomputed independently of our firmware (plain Python) and match
the device log byte-for-byte. A logic analyzer at the device's own SCL/SDA pins shows the transmitted
bytes are exactly the frames listed.

---

## 4. Second, independent failure: the random-number generator returns a constant

This one is unrelated to the CRC question and cannot be attributed to host-side byte order or timing.

`Random` (`0x1B`, no seed — command frame `03 07 1B 00 00 00 24 CD`) was issued twice, reading the
full 32-byte payload both times:

```
read #1 : first 8 payload bytes = FF FF 00 00 FF FF 00 00
read #2 : first 8 payload bytes = FF FF 00 00 FF FF 00 00
=> the two 32-byte payloads are byte-identical, and the pattern repeats inside the payload.
```

Two identical 32-byte random responses are impossible by chance (≈2⁻²⁵⁶), and the value is not merely
"stale" — it is a fixed repeating pattern. A genuine ATECC608B returns a fresh value on every call
(its TRNG is a certified core security function). Please confirm that a genuine ATECC608B cannot
return a fixed pattern here; if that is confirmed, this device's random source is not functional.

---

## 5. Why "this can appear different from some standard CRC-16 implementation" does not cover this case

- We use **your** CRC parameters, verified against the published check vector `0xBB3D` (§2). The
  mismatch is therefore not caused by comparing against a different standard CRC-16 variant.
- The relationship we observe is not "another CRC variant": it is **the very same CRC value with all
  16 bits reversed**. We also tested 13 standard CRC families × 8 byte-coverage/framing hypotheses
  (including init/xorout sweep, and all byte orders): none reproduces the bytes this device sends.
- Swapping the two CRC bytes alone is rejected (0/5). Reversing the bits of each byte alone is
  rejected. **Only "both together" — the 16-bit reversal — is accepted.**
- The device parses non-CRC bytes correctly (it executes five different commands and returns
  structurally correct responses), and its own transmitted bytes are not bit-reversed. So the wire
  bit order is standard; the difference can only be in its CRC computation/check.
- A part that accepted *only its own CRC's bit-reversal* could not be driven by CryptoAuthLib at all,
  because the library computes the CRC exactly as the datasheet specifies.

---

## 6. What we would like from you

1. **Confirmation:** should a genuine ATECC608B accept the Info command
   `03 07 30 00 00 00 BA C0` (the frame CryptoAuthLib's `atcab_info()` builds) after the standard wake
   sequence (SDA low ≥60 µs → release → wait ≥1500 µs), and answer `07 00 00 60 03 …`? If yes, our part
   is non-conforming, and the CRC it demands is not the datasheet CRC.
2. **Confirmation on `Random`:** should a genuine ATECC608B return a **fresh** 32-byte value on every
   `Random` call? Our device returns the same fixed pattern every time (see §4).
3. **Reference-part test:** if you have a genuine part and a host, please run the same three-variant
   test (change only the last two CRC bytes). We expect a genuine part to accept **only the first
   variant**. That single experiment settles it in either direction.
4. **`atcab_info()` proof:** we are willing to run CryptoAuthLib's `atcab_info()` against this part —
   but our lab has no standalone I²C host adapter (no FT232H / Raspberry Pi). We can already state that
   the frame `atcab_info()` generates is byte-identical to variant (a) above: `atca_info()` builds
   `[word 0x03][count 0x07][opcode 0x30][mode][0x00][0x00]` and appends the CRC, and your
   CryptoAuthLib manual states that `atCRC()` returns that two-byte CRC in **little-endian** order.
   With the parameters you list, the CRC over `07 30 00 00 00` is `0xC0BA`, so the frame is
   `03 07 30 00 00 00 BA C0` — which this device rejects. Would a logic-analyzer capture at the device
   pins, plus the exact byte sequences, be acceptable? Or would you prefer we return the part through
   the distributor for direct analysis?

## 7. Reproduction (5 minutes, two writes)

1. 3.3 V, I²C ≤100 kHz, 7-bit address `0x60`, word address `0x03`.
   Wake: SCL high, SDA low ≥60 µs → release → wait ≥1500 µs.
2. Send, observing the first response byte:
   - `03 07 30 00 00 00 BA C0` → measured reply `04 FF 01 42` (**rejected**)
   - `03 07 30 00 00 00 03 5D` → measured reply `07 00 00 60 03 …` (**accepted**)
   - `03 07 30 00 00 00 C0 BA` → measured reply `04 FF 01 42` (**rejected**)
3. Expected for a compliant part: only the **first** of the three is accepted.

---

## Annex (secondary, may be ignored if not relevant)

We have a second unit of the same order which never answered at 0x60. It turned out to be answering at
7-bit address **0x64**, and:

- its `Sign` command returns **`0x03` (parse error)** → it has **no ECC** (an ATECC608B would parse
  Sign and return `0x0F` when the slot has no key);
- its config-zone factory fields match the **ATSHA204A** defaults (RevNum `00 09 04 00`,
  I2C-address field `C8` = 7-bit 0x64, SN[8]=`EE`), whereas a blank ATECC608B ships at 0x60;
- it exhibits the **same bit-reversed CRC** behaviour (`04 11 33 43` on a bare read).

If this second unit is of interest we can prepare a separate report; it suggests the issue is not
limited to a single device.

Best regards,
[your name / company]
[date]

---

### Attachments available on request
- Logic-analyzer decode at the device's own SCL/SDA pins (DSView), showing the transmitted frames
- Our test firmware excerpt (frame construction + CRC) and the host-side Python recomputation script
- Oscilloscope capture of the wake pulse (measured SDA low = 100 µs) and of VCC at the device pins
