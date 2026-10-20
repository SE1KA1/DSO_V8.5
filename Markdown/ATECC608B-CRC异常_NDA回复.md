# Reply to Microchip Support — NDA no longer needed / case closed

**Subject:** Re: ATECC608B-SSHDA-T CRC behaviour — no NDA required, case closed

Dear Microchip Support,

Thank you for the reply and for offering to continue under NDA.

**Please disregard the NDA request — it is no longer needed.** We have found the cause on our
side and are closing the case. Summary:

1. **CRC**: our driver used the wrong CRC variant (CRC-16/ARC). The device accepts the
   standard CRC on 5/5 test commands once the algorithm is corrected to match your
   `atCRC()` (poly `0x8005`, init 0, LSB-first input). **Withdrawn.**
2. **`Random` returning a fixed `FF FF 00 00 …` payload**: this is the documented RNG test
   pattern while the Configuration zone is unlocked. We confirmed
   `LockValue@0x56 = 0x55` / `LockConfig@0x57 = 0x55`. **Withdrawn.**
3. Data-zone / `GenKey` refusals (`0x0F`) follow from the factory-default `SlotConfig` and the
   unlocked state. **Not a defect.**

The device is fully conformant and no RMA or reference-part comparison is required.

If you would like to keep the case open for one line of documentation, a citation of the
datasheet section that describes the pre-lock RNG test pattern would be welcome — but it is
not blocking.

Apologies for the incorrect initial report, and thanks for your time.

Best regards,
Seka
2026-10-20
