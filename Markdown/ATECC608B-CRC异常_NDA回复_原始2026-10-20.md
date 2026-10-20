# Reply to Microchip Support — NDA details + case recap

**Subject:** Re: ATECC608B-SSHDA-T CRC behaviour — NDA details / how to proceed

Dear Microchip Support,

Thank you for the reply. Please find the requested NDA information below.

We do not have an NDA with Microchip at this time. Could you please advise:

1. the correct process and the standard **mutual NDA** template to use, and
2. whether this case can be progressed at the **public datasheet level** in the meantime —
   everything we have shared so far is **our own measurement**; we are not requesting any confidential
   documentation, and we are not asking you to disclose product internals.

We are happy to sign a mutual NDA if that is required to continue.

---

## Case recap (so the thread stays anchored)

**Device:** ATECC608B-SSHDA-T, 7-bit address `0x60`, purchased through an authorized distributor.

**Observed behaviour**

1. The **datasheet CRC** (poly 0x8005 reflected, init 0, CRC bytes LSB first; our implementation matches the
   published check vector `CRC("123456789") = 0xBB3D`) is **rejected** with status `0xFF`. Only the
   **16-bit bit-reversal** of that CRC is accepted.
   Controlled test: 5 different commands (Info / Random / Read32B / Read4B / Nonce, frame lengths 8 B and
   40 B) × 3 variants, **only the last two CRC bytes differ** →
   **std 0/5 accepted, rev16 5/5 accepted, swap 0/5 accepted**.
   The accepted frames are genuinely **executed correctly** (real RevNum `00 00 60 03`, 32 random bytes,
   real configuration-zone data, Nonce status `0x00`).
2. `Random` returns an **identical 32-byte payload on every call**, and the payload is a fixed repeating
   pattern (`FF FF 00 00 …`).
3. Data-zone reads (`Read` 0x02, Param1=`0x82`, Param2=`slot*8`) return **`0x0F`**; `GenKey` returns
   **`0xEE`** at 20/10/5/2 µs half-periods.

**Question:** is this behaviour consistent with a genuine ATECC608B? If not, please advise the next steps
(e.g. a reference-part comparison, or returning the device through the distributor for analysis).

Best regards,
Seka
10.20
