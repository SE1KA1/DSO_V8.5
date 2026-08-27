#!/usr/bin/env python3
"""Dependency-free FMC16 public golden-vector regression."""

GOLDEN_ACK = [
    0xA55A, 0x5AA5, 0x100D, 0x0013, 0x0300, 0x0100, 0x0000, 0x0004,
    0x0000, 0x0001, 0x0000, 0x0001, 0x0141, 0x0030, 0x0001, 0x0001,
    0x0000, 0xF1E8, 0x829E,
]

REQUIRED_CAPABILITIES = 0x00000003


def crc16(words):
    value = 0xFFFF
    for word in words:
        value ^= word
        for _ in range(16):
            value = ((value << 1) ^ 0x1021) & 0xFFFF if value & 0x8000 else (value << 1) & 0xFFFF
    return value


def crc32(words):
    value = 0xFFFFFFFF
    for word in words:
        for bit in range(15, -1, -1):
            feedback = ((value >> 31) ^ (word >> bit)) & 1
            value = ((value << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if feedback else (value << 1) & 0xFFFFFFFF
    return value


def build_get_capabilities(sequence=1, transaction=0x13572468):
    packet = [
        0xA55A, 0x5AA5, 0x100D, 0x0013, 0x3001, 0x0001, 0x0002, 0x0004,
        (sequence >> 16) & 0xFFFF, sequence & 0xFFFF,
        (transaction >> 16) & 0xFFFF, transaction & 0xFFFF,
        0x0000, 0x0001, 0x0100, 0x0000, 0x0000, 0x0000, 0x0000,
    ]
    packet[12] = crc16(packet[2:12])
    packet_crc = crc32(packet[2:17])
    packet[17] = packet_crc >> 16
    packet[18] = packet_crc & 0xFFFF
    return packet


def capabilities_compatible(capabilities):
    return capabilities & REQUIRED_CAPABILITIES == REQUIRED_CAPABILITIES


def main():
    assert crc16(GOLDEN_ACK[2:12]) == 0x0141
    assert crc32(GOLDEN_ACK[2:17]) == 0xF1E8829E
    assert ((GOLDEN_ACK[-2] << 16) | GOLDEN_ACK[-1]) == 0xF1E8829E
    assert GOLDEN_ACK[13:16] == [0x0030, 0x0001, 0x0001]

    command = build_get_capabilities()
    assert len(command) == 19
    assert command[3] == 15 + command[7]
    assert crc16(command[2:12]) == command[12]
    assert crc32(command[2:-2]) == ((command[-2] << 16) | command[-1])
    assert capabilities_compatible(0x00000003)
    assert capabilities_compatible(0x00002BF7)
    assert not capabilities_compatible(0x00000001)
    assert not capabilities_compatible(0x00000002)

    print("CRC16 GOLDEN: PASS")
    print("CRC32 GOLDEN: PASS")
    print("GET_CAPABILITIES PACKET: PASS")
    print("MINIMAL/FULL CAPABILITIES: PASS")
    print("ALL PASS")


if __name__ == "__main__":
    main()
