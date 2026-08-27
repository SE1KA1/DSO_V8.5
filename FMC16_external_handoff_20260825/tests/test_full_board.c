#include "fmc16_full_board.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static size_t make_xo2_packet(uint16_t *packet, uint8_t type, uint16_t channel,
                              const uint16_t *payload, size_t payload_words,
                              uint32_t sequence, uint32_t transaction)
{
    size_t total = 15U + payload_words;
    size_t i;
    uint32_t crc32;

    packet[0] = FMC16_SOF0;
    packet[1] = FMC16_SOF1;
    packet[2] = FMC16_VERSION_HEADER;
    packet[3] = (uint16_t)total;
    packet[4] = (uint16_t)((uint16_t)type << 8);
    packet[5] = 0x0100U;
    packet[6] = channel;
    packet[7] = (uint16_t)payload_words;
    packet[8] = (uint16_t)(sequence >> 16);
    packet[9] = (uint16_t)sequence;
    packet[10] = (uint16_t)(transaction >> 16);
    packet[11] = (uint16_t)transaction;
    packet[12] = fmc16_crc16_words(&packet[2], 10U);
    for (i = 0U; i < payload_words; ++i) packet[13U + i] = payload[i];
    crc32 = fmc16_crc32_words(&packet[2], total - 4U);
    packet[total - 2U] = (uint16_t)(crc32 >> 16);
    packet[total - 1U] = (uint16_t)crc32;
    return total;
}

static int test_builders(void)
{
    uint16_t packet[FMC16_MAX_PACKET_WORDS];
    size_t words = 0U;
    fmc16_host_status_t host;

    CHECK(fmc16_build_get_link_status(packet, FMC16_MAX_PACKET_WORDS,
                                      3U, 0x1001U, FMC16_FLAG_ACK_REQUIRED,
                                      &words) == FMC16_OK);
    CHECK(words == 19U && packet[13] == FMC16_OPCODE_GET_LINK_STATUS);

    CHECK(fmc16_build_get_status_snapshot(packet, FMC16_MAX_PACKET_WORDS,
                                          4U, 0x1002U, 0U, &words) == FMC16_OK);
    CHECK(words == 19U && packet[13] == FMC16_OPCODE_GET_STATUS_SNAPSHOT);

    CHECK(fmc16_build_set_publish_period(packet, FMC16_MAX_PACKET_WORDS,
                                         5U, 0x1003U, 0U,
                                         FMC16_TYPE_IO_SNAPSHOT, 1000U, 0U,
                                         &words) == FMC16_OK);
    CHECK(words == 23U && packet[13] == FMC16_OPCODE_SET_PUBLISH_PERIOD);
    CHECK(packet[17] == FMC16_TYPE_IO_SNAPSHOT && packet[19] == 1000U);
    CHECK(fmc16_build_set_publish_period(packet, FMC16_MAX_PACKET_WORDS,
                                         5U, 0x1003U, 0U,
                                         FMC16_TYPE_IO_SNAPSHOT, 1000U, 1U,
                                         &words) == FMC16_ERR_FIELDS);

    CHECK(fmc16_build_subscribe_events(packet, FMC16_MAX_PACKET_WORDS,
                                       6U, 0x1004U, 0U,
                                       FMC16_EVENT_MASK_PI_APPLIED |
                                       FMC16_EVENT_MASK_PO_CHANGED,
                                       100U, &words) == FMC16_OK);
    CHECK(words == 23U && packet[13] == FMC16_OPCODE_SUBSCRIBE_EVENTS);
    CHECK(packet[18] == 0x8010U && packet[20] == 100U);

    CHECK(fmc16_build_unsubscribe_events(packet, FMC16_MAX_PACKET_WORDS,
                                         7U, 0x1005U, 0U,
                                         FMC16_EVENT_MASK_PO_CHANGED,
                                         &words) == FMC16_OK);
    CHECK(words == 21U && packet[18] == 0x8000U);

    CHECK(fmc16_build_link_resync(packet, FMC16_MAX_PACKET_WORDS,
                                  8U, 0x1006U, 0U, 2U,
                                  0x11223344UL, &words) == FMC16_OK);
    CHECK(words == 22U && packet[17] == 2U);
    CHECK(packet[18] == 0x1122U && packet[19] == 0x3344U);

    CHECK(fmc16_build_io_control_pi(packet, FMC16_MAX_PACKET_WORDS,
                                    9U, 0x1007U, FMC16_FLAG_ACK_REQUIRED,
                                    50U, 0x00FFU, 0x005AU,
                                    FMC16_GENERATION_ANY, 8U,
                                    &words) == FMC16_OK);
    CHECK(words == 28U && packet[13] == FMC16_OPCODE_IO_CONTROL);
    CHECK(packet[15] == 1U && packet[17] == 0x00FFU && packet[18] == 0x005AU);
    CHECK(packet[19] == 0U && packet[20] == 0U && packet[21] == 0U);
    CHECK(fmc16_build_io_control_pi(packet, FMC16_MAX_PACKET_WORDS,
                                    9U, 0x1007U, 0U, 50U, 0U, 0U,
                                    FMC16_GENERATION_ANY, 8U,
                                    &words) == FMC16_ERR_FIELDS);

    memset(&host, 0, sizeof(host));
    host.validity = 0x001FU;
    host.uptime_ms = 1234U;
    host.pi_requested = 0x1357U;
    host.pi_applied = 0x1357U;
    host.pi_generation = 9U;
    host.key_led_mask = 0x000FU;
    host.host_state_flags = 0x000FU;
    host.fmc_service_flags = 0x0007U;
    host.last_applied_transaction_id = 0x11223344UL;
    host.application_schema = FMC16_SCHEMA_V1_0;
    CHECK(fmc16_build_host_status(packet, FMC16_MAX_PACKET_WORDS,
                                  10U, &host, &words) == FMC16_OK);
    CHECK(words == 30U && fmc16_packet_type(packet) == FMC16_TYPE_HOST_STATUS);
    CHECK(packet[5] == 0x0001U && packet[6] == FMC16_CHANNEL_STATUS);
    CHECK(packet[26] == FMC16_SCHEMA_V1_0 && packet[27] == 0U);
    return 0;
}

static int test_parsers(void)
{
    uint16_t packet[FMC16_MAX_PACKET_WORDS];
    size_t words;
    fmc16_link_hello_t hello;
    fmc16_device_info_t device;
    fmc16_reset_notice_t reset;
    fmc16_link_status_t link;
    fmc16_status_snapshot_t status;
    fmc16_io_snapshot_t io;
    fmc16_mode_snapshot_t mode;
    fmc16_event_t event;
    fmc16_response_t response;
    fmc16_capability_report_t capabilities;
    fmc16_io_control_result_t io_result;
    fmc16_ack_t ack;

    const uint16_t hello_p[11] = {
        0x584F, 0x0100, 0x1234, 0x5678, 0x0000, 0x2BF7,
        0x0100, 1, 512, 1024, 1
    };
    const uint16_t device_p[16] = {
        0x0100, 0x584F, 0x0100, 0x0101, 0x1234, 0x5678,
        0x0100, 0x0100, 0x0100, 0, 0, 0x1FFF, 0, 0, 0x001F, 0
    };
    const uint16_t reset_p[8] = {
        0x0100, 1, 0x1234, 0x5678, 0, 2, 3, 0
    };
    const uint16_t link_p[8] = {0x0100, 0x0007, 400, 20, 1, 2, 0, 500};
    const uint16_t status_p[10] = {
        0x0100, 0x0007, 0x1234, 0x5678, 0x9ABC,
        0x1122, 0x3344, 0x55AA, 0, 600
    };
    const uint16_t io_p[19] = {
        0x0100, 0x001F, 0x1357, 0x1357, 0x1122, 0x3344,
        0x55AA, 0x000F, 0x0005, 0x0000, 0, 9, 0, 10,
        0, 11, 0, 700, 0x0007
    };
    const uint16_t mode_p[16] = {
        0x0100, 0x0001, 0x001B, 0x0155, 0x00A1, 0x0100,
        0, 0x0037, 0x1122, 0x3344, 0, 12, 0, 13, 0, 0x0017
    };
    const uint16_t event_p[9] = {
        FMC16_EVENT_PO_CHANGED, 0x0001, 0, 800, 0, 14,
        0x0100, 0x1122, 0x3344
    };
    const uint16_t response_p[10] = {
        FMC16_OPCODE_GET_CAPABILITIES, FMC16_STATUS_OK, 0, 6,
        0, 0x2BF7, 0x0100, 1, 512, 1024
    };
    const uint16_t ack_p[4] = {
        FMC16_TYPE_COMMAND, FMC16_OPCODE_GET_CAPABILITIES,
        FMC16_STATUS_ACCEPTED, 0
    };
    const uint16_t io_response_p[9] = {
        FMC16_OPCODE_IO_CONTROL, FMC16_STATUS_OK, 0, 5,
        0x1357, 0x000F, 0x0005, 0, 9
    };
    const uint16_t link_response_p[11] = {
        FMC16_OPCODE_GET_LINK_STATUS, FMC16_STATUS_OK, 0, 7,
        7, 400, 20, 1, 2, 0, 500
    };
    const uint16_t status_response_p[14] = {
        FMC16_OPCODE_GET_STATUS_SNAPSHOT, FMC16_STATUS_OK, 0, 10,
        0x0100, 7, 0x1234, 0x5678, 0x9ABC,
        0x1122, 0x3344, 0x55AA, 0, 600
    };

    words = make_xo2_packet(packet, FMC16_TYPE_LINK_HELLO,
                            FMC16_CHANNEL_LINK, hello_p, 11U, 1U, 0U);
    CHECK(fmc16_parse_link_hello(packet, words, &hello) == FMC16_OK);
    CHECK(hello.capabilities == FMC16_FULL_CAPABILITIES);
    CHECK(hello.rx_queue_words == 512U && hello.tx_queue_words == 1024U);

    words = make_xo2_packet(packet, FMC16_TYPE_DEVICE_INFO,
                            FMC16_CHANNEL_LINK, device_p, 16U, 2U, 0U);
    CHECK(fmc16_parse_device_info(packet, words, &device) == FMC16_OK);
    CHECK(device.device_type == 0x584FU && device.build_id == 0x12345678UL);
    CHECK(device.supported_mode_mask == 0x00001FFFUL);

    words = make_xo2_packet(packet, FMC16_TYPE_RESET_NOTICE,
                            FMC16_CHANNEL_LINK, reset_p, 8U, 0U, 0U);
    CHECK(fmc16_parse_reset_notice(packet, words, &reset) == FMC16_OK);
    CHECK(reset.reset_reason == 1U && reset.new_boot_id == 2U);

    words = make_xo2_packet(packet, FMC16_TYPE_LINK_STATUS,
                            FMC16_CHANNEL_LINK, link_p, 8U, 2U, 0U);
    CHECK(fmc16_parse_link_status(packet, words, &link) == FMC16_OK);
    CHECK(link.link_flags == 7U && link.uptime_ms == 500U);

    words = make_xo2_packet(packet, FMC16_TYPE_STATUS_SNAPSHOT,
                            FMC16_CHANNEL_STATUS, status_p, 10U, 3U, 0U);
    CHECK(fmc16_parse_status_snapshot(packet, words, &status) == FMC16_OK);
    CHECK(status.po == 0x11223344UL && status.pio == 0x55AAU);

    words = make_xo2_packet(packet, FMC16_TYPE_IO_SNAPSHOT,
                            FMC16_CHANNEL_STATUS, io_p, 19U, 4U, 0U);
    CHECK(fmc16_parse_io_snapshot(packet, words, &io) == FMC16_OK);
    CHECK(io.pi_applied == 0x1357U && io.po == 0x11223344UL);
    CHECK(io.pio == 0x55AAU && io.pio_writable_mask == 0U);
    CHECK(io.pi_generation == 9U && io.business_flags == 7U);

    words = make_xo2_packet(packet, FMC16_TYPE_MODE_SNAPSHOT,
                            FMC16_CHANNEL_STATUS, mode_p, 16U, 5U, 0U);
    CHECK(fmc16_parse_mode_snapshot(packet, words, &mode) == FMC16_OK);
    CHECK(mode.mode == 1U && mode.clk_sel == 27U);
    CHECK(mode.key_state == 0x0155U && mode.experiment_id == 0x00A1U);
    CHECK(mode.legacy_payload == 0x11223344UL);

    words = make_xo2_packet(packet, FMC16_TYPE_EVENT,
                            FMC16_CHANNEL_EVENT, event_p, 9U, 6U, 0U);
    CHECK(fmc16_parse_event(packet, words, &event) == FMC16_OK);
    CHECK(event.event_code == FMC16_EVENT_PO_CHANGED && event.data_words == 2U);
    CHECK(event.data[0] == 0x1122U && event.data[1] == 0x3344U);

    words = make_xo2_packet(packet, FMC16_TYPE_RESPONSE,
                            FMC16_CHANNEL_COMMAND, response_p, 10U,
                            7U, 0x12345678UL);
    CHECK(fmc16_parse_response(packet, words, &response) == FMC16_OK);
    CHECK(response.status == FMC16_STATUS_OK && response.result_words == 6U);
    CHECK(response.result[1] == 0x2BF7U && response.result[5] == 1024U);
    CHECK(fmc16_parse_capability_response(packet, words, &capabilities) == FMC16_OK);
    CHECK(capabilities.capabilities == FMC16_FULL_CAPABILITIES);

    words = make_xo2_packet(packet, FMC16_TYPE_RESPONSE,
                            FMC16_CHANNEL_COMMAND, io_response_p, 9U,
                            8U, 0x12345679UL);
    CHECK(fmc16_parse_io_control_response(packet, words, &io_result) == FMC16_OK);
    CHECK(io_result.pi_applied == 0x1357U && io_result.pi_generation == 9U);
    CHECK(io_result.pio_output_enable == 0x000FU &&
          io_result.pio_drive_value == 0x0005U);

    words = make_xo2_packet(packet, FMC16_TYPE_RESPONSE,
                            FMC16_CHANNEL_COMMAND, link_response_p, 11U,
                            9U, 0x1234567AUL);
    CHECK(fmc16_parse_link_status_response(packet, words, &link) == FMC16_OK);
    CHECK(link.link_flags == 7U && link.uptime_ms == 500U);

    words = make_xo2_packet(packet, FMC16_TYPE_RESPONSE,
                            FMC16_CHANNEL_COMMAND, status_response_p, 14U,
                            10U, 0x1234567BUL);
    CHECK(fmc16_parse_status_snapshot_response(packet, words, &status) == FMC16_OK);
    CHECK(status.po == 0x11223344UL && status.pio == 0x55AAU);

    words = make_xo2_packet(packet, FMC16_TYPE_ACK,
                            FMC16_CHANNEL_COMMAND, ack_p, 4U,
                            11U, 0x12345678UL);
    CHECK(fmc16_parse_ack(packet, words, &ack) == FMC16_OK);
    CHECK(ack.status == FMC16_STATUS_ACCEPTED &&
          ack.original_opcode == FMC16_OPCODE_GET_CAPABILITIES);
    return 0;
}

int main(void)
{
    int result = test_builders();
    if (result != 0) {
        fprintf(stderr, "builder test failed at line %d\n", result);
        return 1;
    }
    result = test_parsers();
    if (result != 0) {
        fprintf(stderr, "parser test failed at line %d\n", result);
        return 2;
    }
    puts("FMC16 FULL BOARD CODEC TEST: ALL PASS");
    return 0;
}
