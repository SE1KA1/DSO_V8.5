#include "fmc16_protocol.h"

#include <stdio.h>
#include <string.h>

static const uint16_t golden_ack[19] = {
    0xA55A, 0x5AA5, 0x100D, 0x0013, 0x0300, 0x0100, 0x0000, 0x0004,
    0x0000, 0x0001, 0x0000, 0x0001, 0x0141, 0x0030, 0x0001, 0x0001,
    0x0000, 0xF1E8, 0x829E
};

int main(void)
{
    uint16_t packet[19];
    size_t words = 0U;
    size_t validated = 0U;

    if (fmc16_crc16_words(&golden_ack[2], 10U) != 0x0141U) {
        return 1;
    }
    if (fmc16_crc32_words(&golden_ack[2], 15U) != 0xF1E8829EUL) {
        return 2;
    }
    if ((golden_ack[13] != FMC16_TYPE_COMMAND) ||
        (golden_ack[14] != FMC16_STATUS_ACCEPTED) ||
        (golden_ack[15] != FMC16_OPCODE_GET_CAPABILITIES)) {
        return 11;
    }
    if (fmc16_validate_packet(golden_ack, 19U, &validated) != FMC16_OK ||
        validated != 19U) {
        return 3;
    }
    if (fmc16_build_get_capabilities(packet, 19U, 1U, 0x13572468UL,
                                     FMC16_FLAG_ACK_REQUIRED, &words) != FMC16_OK ||
        words != 19U) {
        return 4;
    }
    if (fmc16_validate_packet(packet, words, &validated) != FMC16_OK) {
        return 5;
    }

    packet[13] ^= 1U;
    if (fmc16_validate_packet(packet, words, &validated) != FMC16_ERR_PACKET_CRC) {
        return 6;
    }
    memcpy(packet, golden_ack, sizeof(packet));
    packet[12] ^= 1U;
    if (fmc16_validate_packet(packet, words, &validated) != FMC16_ERR_HEADER_CRC) {
        return 7;
    }
    if (!fmc16_capabilities_compatible(0x00000003UL)) {
        return 8;
    }
    if (!fmc16_capabilities_compatible(0x00002BF7UL)) {
        return 9;
    }
    if (fmc16_capabilities_compatible(0x00000001UL) ||
        fmc16_capabilities_compatible(0x00000002UL) ||
        fmc16_capabilities_compatible(0x00000000UL)) {
        return 10;
    }

    puts("FMC16 C PROTOCOL TEST: ALL PASS");
    return 0;
}
