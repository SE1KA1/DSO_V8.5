#include "fmc16_full_board.h"

#include <string.h>

static uint32_t join_u32(uint16_t high, uint16_t low)
{
    return ((uint32_t)high << 16) | low;
}

static fmc16_result_t expect_packet(const uint16_t *packet, size_t words,
                                    uint8_t type, uint16_t channel,
                                    size_t payload_words)
{
    size_t validated = 0U;
    fmc16_result_t result;

    result = fmc16_validate_packet(packet, words, &validated);
    if (result != FMC16_OK) {
        return result;
    }
    if ((validated != words) || (fmc16_packet_type(packet) != type) ||
        (packet[7] != payload_words) || (packet[5] != 0x0100U) ||
        (packet[6] != channel)) {
        return FMC16_ERR_FIELDS;
    }
    return FMC16_OK;
}

static fmc16_result_t build_packet(uint16_t *packet, size_t capacity,
                                   uint8_t type, uint8_t flags, uint16_t channel,
                                   uint32_t sequence, uint32_t transaction,
                                   const uint16_t *payload, size_t payload_words,
                                   size_t *words)
{
    size_t total;
    size_t i;
    uint32_t crc32;

    if ((packet == NULL) || (words == NULL) ||
        ((payload == NULL) && (payload_words != 0U))) {
        return FMC16_ERR_ARGUMENT;
    }
    if ((flags & (uint8_t)~(FMC16_FLAG_ACK_REQUIRED | FMC16_FLAG_RETRY)) != 0U)
        return FMC16_ERR_FIELDS;
    if (payload_words > FMC16_MAX_PAYLOAD_WORDS) {
        return FMC16_ERR_LENGTH;
    }
    total = FMC16_MIN_PACKET_WORDS + payload_words;
    if (capacity < total) {
        return FMC16_ERR_CAPACITY;
    }

    packet[0] = FMC16_SOF0;
    packet[1] = FMC16_SOF1;
    packet[2] = FMC16_VERSION_HEADER;
    packet[3] = (uint16_t)total;
    packet[4] = (uint16_t)(((uint16_t)type << 8) | flags);
    packet[5] = (uint16_t)(((uint16_t)FMC16_NODE_STM32 << 8) | FMC16_NODE_XO2);
    packet[6] = channel;
    packet[7] = (uint16_t)payload_words;
    packet[8] = (uint16_t)(sequence >> 16);
    packet[9] = (uint16_t)sequence;
    packet[10] = (uint16_t)(transaction >> 16);
    packet[11] = (uint16_t)transaction;
    packet[12] = fmc16_crc16_words(&packet[2], 10U);
    for (i = 0U; i < payload_words; ++i) {
        packet[13U + i] = payload[i];
    }
    crc32 = fmc16_crc32_words(&packet[2], total - 4U);
    packet[total - 2U] = (uint16_t)(crc32 >> 16);
    packet[total - 1U] = (uint16_t)crc32;
    *words = total;
    return FMC16_OK;
}

static fmc16_result_t build_command(uint16_t *packet, size_t capacity,
                                    uint32_t sequence, uint32_t transaction,
                                    uint8_t flags, uint16_t opcode,
                                    const uint16_t *arguments, size_t argument_words,
                                    size_t *words)
{
    uint16_t payload[13];
    size_t i;

    if (argument_words > 9U) {
        return FMC16_ERR_LENGTH;
    }
    payload[0] = opcode;
    payload[1] = FMC16_SCHEMA_V1_0;
    payload[2] = 0U;
    payload[3] = 0U;
    for (i = 0U; i < argument_words; ++i) {
        payload[4U + i] = arguments[i];
    }
    return build_packet(packet, capacity, FMC16_TYPE_COMMAND, flags,
                        FMC16_CHANNEL_COMMAND, sequence, transaction,
                        payload, 4U + argument_words, words);
}

fmc16_result_t fmc16_parse_link_hello(const uint16_t *packet, size_t words,
                                      fmc16_link_hello_t *value)
{
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_LINK_HELLO,
                           FMC16_CHANNEL_LINK, 11U);
    if (result != FMC16_OK) return result;
    if ((packet[13] != 0x584FU) || (packet[14] != FMC16_SCHEMA_V1_0))
        return FMC16_ERR_FIELDS;
    value->build_id = join_u32(packet[15], packet[16]);
    value->capabilities = join_u32(packet[17], packet[18]);
    value->max_payload_words = packet[19];
    value->max_outstanding = packet[20];
    value->rx_queue_words = packet[21];
    value->tx_queue_words = packet[22];
    value->reset_reason = packet[23];
    if ((value->max_payload_words == 0U) ||
        (value->max_payload_words > FMC16_MAX_PAYLOAD_WORDS) ||
        (value->max_outstanding == 0U)) return FMC16_ERR_FIELDS;
    return fmc16_capabilities_compatible(value->capabilities) ? FMC16_OK
                                                              : FMC16_ERR_FIELDS;
}

fmc16_result_t fmc16_parse_device_info(const uint16_t *packet, size_t words,
                                       fmc16_device_info_t *value)
{
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_DEVICE_INFO,
                           FMC16_CHANNEL_LINK, 16U);
    if (result != FMC16_OK) return result;
    if ((packet[13] != FMC16_SCHEMA_V1_0) ||
        (packet[15] != FMC16_SCHEMA_V1_0) ||
        (packet[19] != FMC16_SCHEMA_V1_0) ||
        (packet[20] != FMC16_SCHEMA_V1_0) ||
        (packet[21] != FMC16_SCHEMA_V1_0) ||
        (packet[28] != 0U)) return FMC16_ERR_FIELDS;
    value->device_type = packet[14];
    value->role_personality = packet[16];
    value->build_id = join_u32(packet[17], packet[18]);
    value->board_revision = packet[22];
    value->supported_mode_mask = join_u32(packet[23], packet[24]);
    value->supported_experiment_class_mask = join_u32(packet[25], packet[26]);
    value->public_feature_flags = packet[27];
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_reset_notice(const uint16_t *packet, size_t words,
                                        fmc16_reset_notice_t *value)
{
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_RESET_NOTICE,
                           FMC16_CHANNEL_LINK, 8U);
    if (result != FMC16_OK) return result;
    if (packet[13] != FMC16_SCHEMA_V1_0) return FMC16_ERR_FIELDS;
    value->reset_reason = packet[14];
    value->old_build_id = join_u32(packet[15], packet[16]);
    value->new_boot_id = join_u32(packet[17], packet[18]);
    value->invalidated_transactions = packet[19];
    value->flags = packet[20];
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_link_status(const uint16_t *packet, size_t words,
                                       fmc16_link_status_t *value)
{
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_LINK_STATUS,
                           FMC16_CHANNEL_LINK, 8U);
    if (result != FMC16_OK) return result;
    if ((packet[13] != FMC16_SCHEMA_V1_0) || ((packet[14] & 0xFFF0U) != 0U))
        return FMC16_ERR_FIELDS;
    value->link_flags = packet[14];
    value->rx_free_words = packet[15];
    value->tx_used_words = packet[16];
    value->rx_free_packet_slots = packet[17];
    value->tx_pending_packets = packet[18];
    value->uptime_ms = join_u32(packet[19], packet[20]);
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_status_snapshot(const uint16_t *packet, size_t words,
                                           fmc16_status_snapshot_t *value)
{
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_STATUS_SNAPSHOT,
                           FMC16_CHANNEL_STATUS, 10U);
    if (result != FMC16_OK) return result;
    if ((packet[13] != FMC16_SCHEMA_V1_0) || ((packet[14] & 0xFFF8U) != 0U))
        return FMC16_ERR_FIELDS;
    value->validity = packet[14];
    value->legacy_words[0] = packet[15];
    value->legacy_words[1] = packet[16];
    value->legacy_words[2] = packet[17];
    value->po = join_u32(packet[18], packet[19]);
    value->pio = packet[20];
    value->timestamp_ms = join_u32(packet[21], packet[22]);
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_io_snapshot(const uint16_t *packet, size_t words,
                                       fmc16_io_snapshot_t *value)
{
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_IO_SNAPSHOT,
                           FMC16_CHANNEL_STATUS, 19U);
    if (result != FMC16_OK) return result;
    if ((packet[13] != FMC16_SCHEMA_V1_0) || ((packet[14] & 0xFCE0U) != 0U) ||
        ((packet[31] & 0xFFF8U) != 0U)) return FMC16_ERR_FIELDS;
    value->validity = packet[14];
    value->pi_requested = packet[15];
    value->pi_applied = packet[16];
    value->po = join_u32(packet[17], packet[18]);
    value->pio = packet[19];
    value->pio_output_enable = packet[20];
    value->pio_drive_value = packet[21];
    value->pio_writable_mask = packet[22];
    value->pi_generation = join_u32(packet[23], packet[24]);
    value->po_generation = join_u32(packet[25], packet[26]);
    value->pio_generation = join_u32(packet[27], packet[28]);
    value->timestamp_ms = join_u32(packet[29], packet[30]);
    value->business_flags = packet[31];
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_mode_snapshot(const uint16_t *packet, size_t words,
                                         fmc16_mode_snapshot_t *value)
{
    uint16_t mode_owner;
    uint16_t clock;
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = expect_packet(packet, words, FMC16_TYPE_MODE_SNAPSHOT,
                           FMC16_CHANNEL_STATUS, 16U);
    if (result != FMC16_OK) return result;
    if ((packet[13] != FMC16_SCHEMA_V1_0) ||
        ((packet[14] & 0xFCF0U) != 0U) ||
        ((packet[15] & 0xFF00U) != 0U) ||
        ((packet[16] & 0xFC00U) != 0U) ||
        (packet[18] != FMC16_SCHEMA_V1_0) ||
        ((packet[20] & 0xFFC0U) != 0U) ||
        ((packet[28] & 0xFFE0U) != 0U)) return FMC16_ERR_FIELDS;
    mode_owner = packet[14];
    clock = packet[15];
    value->mode = (uint8_t)(mode_owner & 0x0FU);
    value->owner = (uint8_t)((mode_owner >> 8) & 0x03U);
    value->clk_sel = (uint8_t)(clock & 0x1FU);
    value->full_select = (uint8_t)((clock >> 5) & 1U);
    value->pll_locked = (uint8_t)((clock >> 6) & 1U);
    value->clock_output_connected = (uint8_t)((clock >> 7) & 1U);
    value->key_state = (uint16_t)(packet[16] & 0x03FFU);
    value->experiment_id = packet[17];
    value->experiment_schema = packet[18];
    value->experiment_page_id = packet[19];
    value->business_validity = packet[20];
    value->legacy_payload = join_u32(packet[21], packet[22]);
    value->mode_generation = join_u32(packet[23], packet[24]);
    value->clock_generation = join_u32(packet[25], packet[26]);
    value->control_context = packet[27];
    value->flags = packet[28];
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_event(const uint16_t *packet, size_t words,
                                 fmc16_event_t *value)
{
    size_t data_words;
    size_t i;
    size_t validated = 0U;
    fmc16_result_t result;
    if ((packet == NULL) || (value == NULL)) return FMC16_ERR_ARGUMENT;
    result = fmc16_validate_packet(packet, words, &validated);
    if (result != FMC16_OK) return result;
    if ((validated != words) || (fmc16_packet_type(packet) != FMC16_TYPE_EVENT) ||
        (packet[5] != 0x0100U) || (packet[6] != FMC16_CHANNEL_EVENT) ||
        (packet[7] < 7U)) return FMC16_ERR_FIELDS;
    data_words = packet[7] - 7U;
    if ((data_words > FMC16_MAX_EVENT_DATA_WORDS) ||
        (packet[19] != FMC16_SCHEMA_V1_0)) return FMC16_ERR_FIELDS;
    if ((packet[13] != FMC16_EVENT_MODE_CHANGED) &&
        (packet[13] != FMC16_EVENT_CLOCK_CHANGED) &&
        (packet[13] != FMC16_EVENT_KEY_CHANGED) &&
        (packet[13] != FMC16_EVENT_EXPERIMENT_CHANGED) &&
        (packet[13] != FMC16_EVENT_PI_APPLIED) &&
        (packet[13] != FMC16_EVENT_PO_CHANGED) &&
        (packet[13] != FMC16_EVENT_PIO_CHANGED)) return FMC16_ERR_FIELDS;
    if ((((packet[13] == FMC16_EVENT_MODE_CHANGED) ||
          (packet[13] == FMC16_EVENT_CLOCK_CHANGED) ||
          (packet[13] == FMC16_EVENT_KEY_CHANGED) ||
          (packet[13] == FMC16_EVENT_EXPERIMENT_CHANGED) ||
          (packet[13] == FMC16_EVENT_PIO_CHANGED)) && (data_words != 1U)) ||
        ((packet[13] == FMC16_EVENT_PI_APPLIED) && (data_words != 3U)) ||
        ((packet[13] == FMC16_EVENT_PO_CHANGED) && (data_words != 2U)))
        return FMC16_ERR_FIELDS;
    memset(value, 0, sizeof(*value));
    value->event_code = packet[13];
    value->event_source = packet[14];
    value->timestamp_ms = join_u32(packet[15], packet[16]);
    value->event_generation = join_u32(packet[17], packet[18]);
    value->data_words = (uint16_t)data_words;
    for (i = 0U; i < data_words; ++i) value->data[i] = packet[20U + i];
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_response(const uint16_t *packet, size_t words,
                                    fmc16_response_t *value)
{
    size_t i;
    size_t validated = 0U;
    fmc16_result_t result;
    if ((packet == NULL) || (value == NULL)) return FMC16_ERR_ARGUMENT;
    result = fmc16_validate_packet(packet, words, &validated);
    if (result != FMC16_OK) return result;
    if ((validated != words) || (fmc16_packet_type(packet) != FMC16_TYPE_RESPONSE) ||
        (packet[5] != 0x0100U) || (packet[6] != FMC16_CHANNEL_COMMAND) ||
        (packet[7] < 4U) ||
        (packet[16] != (packet[7] - 4U)) ||
        (packet[16] > FMC16_MAX_RESPONSE_RESULTS)) return FMC16_ERR_FIELDS;
    memset(value, 0, sizeof(*value));
    value->original_opcode = packet[13];
    value->status = packet[14];
    value->detail = packet[15];
    value->result_words = packet[16];
    for (i = 0U; i < value->result_words; ++i) value->result[i] = packet[17U + i];
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_capability_response(const uint16_t *packet, size_t words,
                                               fmc16_capability_report_t *value)
{
    fmc16_response_t response;
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = fmc16_parse_response(packet, words, &response);
    if (result != FMC16_OK) return result;
    if ((response.original_opcode != FMC16_OPCODE_GET_CAPABILITIES) ||
        (response.status != FMC16_STATUS_OK) || (response.result_words != 6U))
        return FMC16_ERR_FIELDS;
    value->capabilities = join_u32(response.result[0], response.result[1]);
    value->max_payload_words = response.result[2];
    value->max_outstanding = response.result[3];
    value->rx_queue_words = response.result[4];
    value->tx_queue_words = response.result[5];
    return fmc16_capabilities_compatible(value->capabilities) ? FMC16_OK
                                                              : FMC16_ERR_FIELDS;
}

fmc16_result_t fmc16_parse_link_status_response(const uint16_t *packet, size_t words,
                                                fmc16_link_status_t *value)
{
    fmc16_response_t response;
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = fmc16_parse_response(packet, words, &response);
    if (result != FMC16_OK) return result;
    if ((response.original_opcode != FMC16_OPCODE_GET_LINK_STATUS) ||
        (response.status != FMC16_STATUS_OK) || (response.result_words != 7U) ||
        ((response.result[0] & 0xFFF0U) != 0U)) return FMC16_ERR_FIELDS;
    value->link_flags = response.result[0];
    value->rx_free_words = response.result[1];
    value->tx_used_words = response.result[2];
    value->rx_free_packet_slots = response.result[3];
    value->tx_pending_packets = response.result[4];
    value->uptime_ms = join_u32(response.result[5], response.result[6]);
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_status_snapshot_response(
    const uint16_t *packet, size_t words, fmc16_status_snapshot_t *value)
{
    fmc16_response_t response;
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = fmc16_parse_response(packet, words, &response);
    if (result != FMC16_OK) return result;
    if ((response.original_opcode != FMC16_OPCODE_GET_STATUS_SNAPSHOT) ||
        (response.status != FMC16_STATUS_OK) || (response.result_words != 10U) ||
        (response.result[0] != FMC16_SCHEMA_V1_0) ||
        ((response.result[1] & 0xFFF8U) != 0U)) return FMC16_ERR_FIELDS;
    value->validity = response.result[1];
    value->legacy_words[0] = response.result[2];
    value->legacy_words[1] = response.result[3];
    value->legacy_words[2] = response.result[4];
    value->po = join_u32(response.result[5], response.result[6]);
    value->pio = response.result[7];
    value->timestamp_ms = join_u32(response.result[8], response.result[9]);
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_io_control_response(const uint16_t *packet, size_t words,
                                               fmc16_io_control_result_t *value)
{
    fmc16_response_t response;
    fmc16_result_t result;
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    result = fmc16_parse_response(packet, words, &response);
    if (result != FMC16_OK) return result;
    if ((response.original_opcode != FMC16_OPCODE_IO_CONTROL) ||
        (response.status != FMC16_STATUS_OK) || (response.result_words != 5U))
        return FMC16_ERR_FIELDS;
    value->pi_applied = response.result[0];
    value->pio_output_enable = response.result[1];
    value->pio_drive_value = response.result[2];
    value->pi_generation = join_u32(response.result[3], response.result[4]);
    return FMC16_OK;
}

fmc16_result_t fmc16_parse_ack(const uint16_t *packet, size_t words,
                               fmc16_ack_t *value)
{
    uint8_t type;
    fmc16_result_t result;
    if ((packet == NULL) || (value == NULL)) return FMC16_ERR_ARGUMENT;
    type = fmc16_packet_type(packet);
    if ((type != FMC16_TYPE_ACK) && (type != FMC16_TYPE_NACK))
        return FMC16_ERR_FIELDS;
    result = expect_packet(packet, words, type, FMC16_CHANNEL_COMMAND, 4U);
    if (result != FMC16_OK) return result;
    value->original_type = packet[13];
    value->original_opcode = packet[14];
    value->status = packet[15];
    value->detail = packet[16];
    return FMC16_OK;
}

fmc16_result_t fmc16_build_get_link_status(uint16_t *packet, size_t capacity,
                                           uint32_t sequence, uint32_t transaction,
                                           uint8_t flags, size_t *words)
{
    return build_command(packet, capacity, sequence, transaction, flags,
                         FMC16_OPCODE_GET_LINK_STATUS, NULL, 0U, words);
}

fmc16_result_t fmc16_build_get_status_snapshot(uint16_t *packet, size_t capacity,
                                               uint32_t sequence, uint32_t transaction,
                                               uint8_t flags, size_t *words)
{
    return build_command(packet, capacity, sequence, transaction, flags,
                         FMC16_OPCODE_GET_STATUS_SNAPSHOT, NULL, 0U, words);
}

fmc16_result_t fmc16_build_set_publish_period(uint16_t *packet, size_t capacity,
                                              uint32_t sequence, uint32_t transaction,
                                              uint8_t flags, uint16_t message_type,
                                              uint32_t period_ms, uint16_t threshold,
                                              size_t *words)
{
    uint16_t args[4] = {message_type, (uint16_t)(period_ms >> 16),
                        (uint16_t)period_ms, threshold};
    if (((message_type != FMC16_TYPE_LINK_STATUS) &&
         (message_type != FMC16_TYPE_STATUS_SNAPSHOT) &&
         (message_type != FMC16_TYPE_IO_SNAPSHOT) &&
         (message_type != FMC16_TYPE_MODE_SNAPSHOT)) || (threshold != 0U))
        return FMC16_ERR_FIELDS;
    return build_command(packet, capacity, sequence, transaction, flags,
                         FMC16_OPCODE_SET_PUBLISH_PERIOD, args, 4U, words);
}

fmc16_result_t fmc16_build_subscribe_events(uint16_t *packet, size_t capacity,
                                            uint32_t sequence, uint32_t transaction,
                                            uint8_t flags, uint32_t event_mask,
                                            uint32_t min_interval_ms, size_t *words)
{
    uint16_t args[4];
    if ((event_mask & ~FMC16_EVENT_MASK_ALL) != 0UL) return FMC16_ERR_FIELDS;
    args[0] = (uint16_t)(event_mask >> 16);
    args[1] = (uint16_t)event_mask;
    args[2] = (uint16_t)(min_interval_ms >> 16);
    args[3] = (uint16_t)min_interval_ms;
    return build_command(packet, capacity, sequence, transaction, flags,
                         FMC16_OPCODE_SUBSCRIBE_EVENTS, args, 4U, words);
}

fmc16_result_t fmc16_build_unsubscribe_events(uint16_t *packet, size_t capacity,
                                              uint32_t sequence, uint32_t transaction,
                                              uint8_t flags, uint32_t event_mask,
                                              size_t *words)
{
    uint16_t args[2];
    if ((event_mask & ~FMC16_EVENT_MASK_ALL) != 0UL) return FMC16_ERR_FIELDS;
    args[0] = (uint16_t)(event_mask >> 16);
    args[1] = (uint16_t)event_mask;
    return build_command(packet, capacity, sequence, transaction, flags,
                         FMC16_OPCODE_UNSUBSCRIBE_EVENTS, args, 2U, words);
}

fmc16_result_t fmc16_build_link_resync(uint16_t *packet, size_t capacity,
                                       uint32_t sequence, uint32_t transaction,
                                       uint8_t flags, uint16_t reason,
                                       uint32_t last_good_sequence, size_t *words)
{
    uint16_t args[3] = {reason, (uint16_t)(last_good_sequence >> 16),
                        (uint16_t)last_good_sequence};
    return build_command(packet, capacity, sequence, transaction, flags,
                         FMC16_OPCODE_LINK_RESYNC, args, 3U, words);
}

fmc16_result_t fmc16_build_io_control_pi(uint16_t *packet, size_t capacity,
                                         uint32_t sequence, uint32_t transaction,
                                         uint8_t flags, uint16_t timeout_ms,
                                         uint16_t pi_update_mask, uint16_t pi_value,
                                         uint32_t expected_generation,
                                         uint32_t new_generation, size_t *words)
{
    uint16_t payload[13] = {FMC16_OPCODE_IO_CONTROL, FMC16_SCHEMA_V1_0, 1U,
                            timeout_ms, pi_update_mask, pi_value,
                            0U, 0U, 0U,
                            (uint16_t)(expected_generation >> 16),
                            (uint16_t)expected_generation,
                            (uint16_t)(new_generation >> 16),
                            (uint16_t)new_generation};
    if (pi_update_mask == 0U) return FMC16_ERR_FIELDS;
    return build_packet(packet, capacity, FMC16_TYPE_COMMAND, flags,
                        FMC16_CHANNEL_COMMAND, sequence, transaction,
                        payload, 13U, words);
}

fmc16_result_t fmc16_build_host_status(uint16_t *packet, size_t capacity,
                                       uint32_t sequence,
                                       const fmc16_host_status_t *value,
                                       size_t *words)
{
    uint16_t payload[15];
    if (value == NULL) return FMC16_ERR_ARGUMENT;
    if (((value->validity & 0xFFE0U) != 0U) ||
        ((value->host_state_flags & 0xFFF0U) != 0U) ||
        ((value->fmc_service_flags & 0xFFF8U) != 0U) ||
        (value->application_schema != FMC16_SCHEMA_V1_0)) return FMC16_ERR_FIELDS;
    payload[0] = FMC16_SCHEMA_V1_0;
    payload[1] = value->validity;
    payload[2] = (uint16_t)(value->uptime_ms >> 16);
    payload[3] = (uint16_t)value->uptime_ms;
    payload[4] = value->pi_requested;
    payload[5] = value->pi_applied;
    payload[6] = (uint16_t)(value->pi_generation >> 16);
    payload[7] = (uint16_t)value->pi_generation;
    payload[8] = value->key_led_mask;
    payload[9] = value->host_state_flags;
    payload[10] = value->fmc_service_flags;
    payload[11] = (uint16_t)(value->last_applied_transaction_id >> 16);
    payload[12] = (uint16_t)value->last_applied_transaction_id;
    payload[13] = value->application_schema;
    payload[14] = 0U;
    return build_packet(packet, capacity, FMC16_TYPE_HOST_STATUS, 0U,
                        FMC16_CHANNEL_STATUS, sequence, 0U,
                        payload, 15U, words);
}
