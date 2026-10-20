# To Microchip Support — Case closure: both reported anomalies were our own error

**Subject:** Re: ATECC608B-SSHDA-T — correction and case closure (no defect found)

---

Dear Microchip Support,

Please close this case. After your team's reply and our own further testing, we found that
**both anomalies we reported were caused by our host implementation, not by the device.**
We would like to withdraw both claims and explain what we found, so that the case record is
accurate.

## 1. CRC behaviour — withdrawn (our error)

We originally reported that the device rejected the datasheet CRC and accepted only its
16-bit reversal.

**What was wrong:** our driver implemented **CRC-16/ARC** (reflected polynomial `0xA001`,
init 0). We verified it against the published check vector `CRC("123456789") = 0xBB3D`, which
made us believe the algorithm was correct.

**What is correct:** the ATECC CRC is polynomial `0x8005`, init `0`, with each input byte
processed **LSB-first** (i.e. non-reflected `0x8005` over bit-reversed input). We confirmed
this against Microchip's own `atCRC()` exported by the installed CryptoAuthLib binary:

```
atCRC(04 11)          = 0x4333
atCRC(04 FF)          = 0x4201
atCRC(07 30 00 00 00) = 0x5D03
```

After correcting the driver, the device accepts the standard CRC on **5 out of 5** different
commands (Info / Random / Read-32B / Read-4B / Nonce; frame lengths 8 B and 40 B), while
three deliberately wrong variants are rejected 0/5 each. **The device is fully conformant.**

## 2. Fixed RNG output — withdrawn (documented pre-lock behaviour)

We reported that `Random` returned an identical, fixed 32-byte payload (`FF FF 00 00 …`) on
every call, which looked like a missing TRNG.

**What we missed:** per the datasheet, while the **Configuration zone is unlocked** the RNG
returns exactly this test pattern (to simplify production testing); the true TRNG is available
only after the Configuration zone is locked. We read back the lock bytes and confirmed the part
is in that factory state:

```
Config[0x56] LockValue  = 0x55   (unlocked)
Config[0x57] LockConfig = 0x55   (unlocked)
Config[0x58..0x59] SlotLocked = FF FF
Info RevNum = 00 00 60 03
```

So the observation matches the specification exactly. **No anomaly.**

## 3. Other observations — explained, not defects

- Data-zone read/write and `GenKey` return `0x0F`. With the factory-default `SlotConfig`
  (e.g. Slot0 `0x2083`: `WriteConfig=2`, `ReadKey=3`; Slot5 `0x8F8F`: `ReadKey=15`) and an
  unlocked Configuration zone, these refusals are expected. Not a defect.

## 4. Requests

1. Please **note the correction in the case record** and close it.
2. **No NDA is required** for our side — the issue is resolved and we are not requesting any
   confidential documentation.
3. If convenient, a one-line confirmation that the pre-lock RNG test pattern is the documented
   behaviour would be helpful for our internal documentation. If a datasheet section reference
   is available, we will cite it.

Thank you for your time and for the earlier pointers — they helped us find the real cause on
our side.

Best regards,
Seka
2026-10-20

---

*Original submission (2026-10-13) and the earlier NDA-related reply are kept as internal
records only (`ATECC608B-CRC异常_回复Microchip_原始2026-10-13.md`,
`ATECC608B-CRC异常_NDA回复_原始2026-10-20.md`).*
