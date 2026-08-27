#include "fmc16_port_stm32h743.h"
#include "fmc16_protocol.h"
#include "fmc16_smoke.h"

#define FMC16_CAPTURE_LIMIT 500000UL

typedef enum {
    FMC16_SMOKE_OK = 0,
    FMC16_SMOKE_PORT = -200,
    FMC16_SMOKE_RESET_NOTICE = -201,
    FMC16_SMOKE_LINK_HELLO = -202,
    FMC16_SMOKE_DEVICE_INFO = -203,
    FMC16_SMOKE_COMMAND = -204,
    FMC16_SMOKE_ACK = -205,
    FMC16_SMOKE_RESPONSE = -206
} fmc16_smoke_result_t;

static uint16_t packet[FMC16_MAX_PACKET_WORDS];

static int capture_type(uint8_t expected_type, size_t expected_words,
                        uint32_t expected_sequence)
{
    size_t words = 0U;
    fmc16_result_t result;

    result = fmc16_capture_packet(fmc16_stm32h743_read_word, NULL,
                                  packet, FMC16_MAX_PACKET_WORDS,
                                  FMC16_CAPTURE_LIMIT, &words);
    if (result != FMC16_OK) {
        return -1;
    }
    if ((words != expected_words) ||
        (fmc16_packet_type(packet) != expected_type) ||
        (fmc16_packet_sequence(packet) != expected_sequence)) {
        return -1;
    }
    return 0;
}

int fmc16_minimal_smoke_run_with_capabilities(uint32_t *peer_capabilities)
{
    const uint32_t transaction_id = 0x13572468UL;
    uint32_t hello_capabilities;
    uint32_t response_capabilities;
    size_t words = 0U;
    fmc16_result_t result;

    if (peer_capabilities != NULL) {
        *peer_capabilities = 0U;
    }

    fmc16_stm32h743_mpu_config();
    if (fmc16_stm32h743_init() != FMC16_STM32_OK) {
        return FMC16_SMOKE_PORT;
    }

    if (capture_type(FMC16_TYPE_RESET_NOTICE, 23U, 0U) != 0) {
        return FMC16_SMOKE_RESET_NOTICE;
    }
    if (capture_type(FMC16_TYPE_LINK_HELLO, 26U, 1U) != 0) {
        return FMC16_SMOKE_LINK_HELLO;
    }
    hello_capabilities = ((uint32_t)packet[17] << 16) | packet[18];
    if ((packet[13] != 0x584FU) ||
        !fmc16_capabilities_compatible(hello_capabilities)) {
        return FMC16_SMOKE_LINK_HELLO;
    }
    if (capture_type(FMC16_TYPE_DEVICE_INFO, 31U, 2U) != 0) {
        return FMC16_SMOKE_DEVICE_INFO;
    }

    result = fmc16_build_get_capabilities(packet, FMC16_MAX_PACKET_WORDS,
                                          1U, transaction_id,
                                          FMC16_FLAG_ACK_REQUIRED, &words);
    if ((result != FMC16_OK) ||
        (fmc16_write_packet(fmc16_stm32h743_write_word, NULL,
                            packet, words) != FMC16_OK)) {
        return FMC16_SMOKE_COMMAND;
    }

    result = fmc16_capture_packet(fmc16_stm32h743_read_word, NULL,
                                  packet, FMC16_MAX_PACKET_WORDS,
                                  FMC16_CAPTURE_LIMIT, &words);
    if ((result != FMC16_OK) || (words != 19U) ||
        (fmc16_packet_type(packet) != FMC16_TYPE_ACK) ||
        (fmc16_packet_transaction(packet) != transaction_id) ||
        (packet[13] != FMC16_TYPE_COMMAND) ||
        (packet[14] != FMC16_STATUS_ACCEPTED) ||
        (packet[15] != FMC16_OPCODE_GET_CAPABILITIES)) {
        return FMC16_SMOKE_ACK;
    }

    result = fmc16_capture_packet(fmc16_stm32h743_read_word, NULL,
                                  packet, FMC16_MAX_PACKET_WORDS,
                                  FMC16_CAPTURE_LIMIT, &words);
    if ((result != FMC16_OK) || (words != 25U)) {
        return FMC16_SMOKE_RESPONSE;
    }
    response_capabilities = ((uint32_t)packet[17] << 16) | packet[18];
    if ((fmc16_packet_type(packet) != FMC16_TYPE_RESPONSE) ||
        (fmc16_packet_transaction(packet) != transaction_id) ||
        (packet[13] != FMC16_OPCODE_GET_CAPABILITIES) ||
        (packet[14] != FMC16_STATUS_OK) ||
        (packet[16] != 6U) ||
        !fmc16_capabilities_compatible(response_capabilities) ||
        (response_capabilities != hello_capabilities)) {
        return FMC16_SMOKE_RESPONSE;
    }
    if (peer_capabilities != NULL) {
        *peer_capabilities = response_capabilities;
    }
    return FMC16_SMOKE_OK;
}

int fmc16_minimal_smoke_run(void)
{
    return fmc16_minimal_smoke_run_with_capabilities(NULL);
}
