#ifndef FMC16_FULL_BOARD_H
#define FMC16_FULL_BOARD_H

#include "fmc16_protocol.h"

#define FMC16_SCHEMA_V1_0                0x0100U
#define FMC16_FULL_CAPABILITIES          0x00002BF7UL

#define FMC16_TYPE_LINK_STATUS           0x02U
#define FMC16_TYPE_STATUS_SNAPSHOT       0x10U
#define FMC16_TYPE_IO_SNAPSHOT           0x11U
#define FMC16_TYPE_MODE_SNAPSHOT         0x12U
#define FMC16_TYPE_HOST_STATUS           0x13U
#define FMC16_TYPE_EVENT                 0x20U

#define FMC16_OPCODE_GET_LINK_STATUS     0x0002U
#define FMC16_OPCODE_GET_STATUS_SNAPSHOT 0x0003U
#define FMC16_OPCODE_SET_PUBLISH_PERIOD  0x0012U
#define FMC16_OPCODE_SUBSCRIBE_EVENTS    0x0020U
#define FMC16_OPCODE_UNSUBSCRIBE_EVENTS  0x0021U
#define FMC16_OPCODE_LINK_RESYNC         0x00F0U
#define FMC16_OPCODE_IO_CONTROL          0x0100U

#define FMC16_EVENT_MODE_CHANGED         0x0001U
#define FMC16_EVENT_CLOCK_CHANGED        0x0002U
#define FMC16_EVENT_KEY_CHANGED          0x0003U
#define FMC16_EVENT_EXPERIMENT_CHANGED   0x0004U
#define FMC16_EVENT_PI_APPLIED           0x0005U
#define FMC16_EVENT_PO_CHANGED           0x0010U
#define FMC16_EVENT_PIO_CHANGED          0x0011U

#define FMC16_EVENT_MASK_MODE_CHANGED       (1UL << 0)
#define FMC16_EVENT_MASK_CLOCK_CHANGED      (1UL << 1)
#define FMC16_EVENT_MASK_KEY_CHANGED        (1UL << 2)
#define FMC16_EVENT_MASK_EXPERIMENT_CHANGED (1UL << 3)
#define FMC16_EVENT_MASK_PI_APPLIED         (1UL << 4)
#define FMC16_EVENT_MASK_PO_CHANGED         (1UL << 15)
#define FMC16_EVENT_MASK_PIO_CHANGED        (1UL << 16)
#define FMC16_EVENT_MASK_ALL                0x0001801FUL

#define FMC16_STATUS_BAD_LENGTH          0x0003U
#define FMC16_STATUS_BAD_ARGUMENT        0x0004U
#define FMC16_STATUS_BUSY                0x0005U
#define FMC16_STATUS_NO_CREDIT           0x0006U
#define FMC16_STATUS_TIMEOUT             0x0007U
#define FMC16_STATUS_CRC_ERROR           0x0008U
#define FMC16_STATUS_UNSUPPORTED_VERSION 0x0009U
#define FMC16_STATUS_INTERNAL_ERROR      0x000AU
#define FMC16_STATUS_READ_ONLY           0x000BU
#define FMC16_STATUS_OWNERSHIP_CONFLICT  0x000CU
#define FMC16_STATUS_STALE_GENERATION    0x000DU
#define FMC16_GENERATION_ANY             0xFFFFFFFFUL
#define FMC16_MAX_RESPONSE_RESULTS       16U
#define FMC16_MAX_EVENT_DATA_WORDS       3U

typedef struct {
    uint32_t build_id;
    uint32_t capabilities;
    uint16_t max_payload_words;
    uint16_t max_outstanding;
    uint16_t rx_queue_words;
    uint16_t tx_queue_words;
    uint16_t reset_reason;
} fmc16_link_hello_t;

typedef struct {
    uint16_t device_type;
    uint16_t role_personality;
    uint32_t build_id;
    uint16_t board_revision;
    uint32_t supported_mode_mask;
    uint32_t supported_experiment_class_mask;
    uint16_t public_feature_flags;
} fmc16_device_info_t;

typedef struct {
    uint16_t reset_reason;
    uint32_t old_build_id;
    uint32_t new_boot_id;
    uint16_t invalidated_transactions;
    uint16_t flags;
} fmc16_reset_notice_t;

typedef struct {
    uint16_t link_flags;
    uint16_t rx_free_words;
    uint16_t tx_used_words;
    uint16_t rx_free_packet_slots;
    uint16_t tx_pending_packets;
    uint32_t uptime_ms;
} fmc16_link_status_t;

typedef struct {
    uint16_t validity;
    uint16_t legacy_words[3];
    uint32_t po;
    uint16_t pio;
    uint32_t timestamp_ms;
} fmc16_status_snapshot_t;

typedef struct {
    uint16_t validity;
    uint16_t pi_requested;
    uint16_t pi_applied;
    uint32_t po;
    uint16_t pio;
    uint16_t pio_output_enable;
    uint16_t pio_drive_value;
    uint16_t pio_writable_mask;
    uint32_t pi_generation;
    uint32_t po_generation;
    uint32_t pio_generation;
    uint32_t timestamp_ms;
    uint16_t business_flags;
} fmc16_io_snapshot_t;

typedef struct {
    uint8_t mode;
    uint8_t owner;
    uint8_t clk_sel;
    uint8_t full_select;
    uint8_t pll_locked;
    uint8_t clock_output_connected;
    uint16_t key_state;
    uint16_t experiment_id;
    uint16_t experiment_schema;
    uint16_t experiment_page_id;
    uint16_t business_validity;
    uint32_t legacy_payload;
    uint32_t mode_generation;
    uint32_t clock_generation;
    uint16_t control_context;
    uint16_t flags;
} fmc16_mode_snapshot_t;

typedef struct {
    uint16_t event_code;
    uint16_t event_source;
    uint32_t timestamp_ms;
    uint32_t event_generation;
    uint16_t data_words;
    uint16_t data[FMC16_MAX_EVENT_DATA_WORDS];
} fmc16_event_t;

typedef struct {
    uint16_t original_opcode;
    uint16_t status;
    uint16_t detail;
    uint16_t result_words;
    uint16_t result[FMC16_MAX_RESPONSE_RESULTS];
} fmc16_response_t;

typedef struct {
    uint32_t capabilities;
    uint16_t max_payload_words;
    uint16_t max_outstanding;
    uint16_t rx_queue_words;
    uint16_t tx_queue_words;
} fmc16_capability_report_t;

typedef struct {
    uint16_t pi_applied;
    uint16_t pio_output_enable;
    uint16_t pio_drive_value;
    uint32_t pi_generation;
} fmc16_io_control_result_t;

typedef struct {
    uint16_t original_type;
    uint16_t original_opcode;
    uint16_t status;
    uint16_t detail;
} fmc16_ack_t;

typedef struct {
    uint16_t validity;
    uint32_t uptime_ms;
    uint16_t pi_requested;
    uint16_t pi_applied;
    uint32_t pi_generation;
    uint16_t key_led_mask;
    uint16_t host_state_flags;
    uint16_t fmc_service_flags;
    uint32_t last_applied_transaction_id;
    uint16_t application_schema;
} fmc16_host_status_t;

fmc16_result_t fmc16_parse_link_hello(const uint16_t *packet, size_t words,
                                      fmc16_link_hello_t *value);
fmc16_result_t fmc16_parse_device_info(const uint16_t *packet, size_t words,
                                       fmc16_device_info_t *value);
fmc16_result_t fmc16_parse_reset_notice(const uint16_t *packet, size_t words,
                                        fmc16_reset_notice_t *value);
fmc16_result_t fmc16_parse_link_status(const uint16_t *packet, size_t words,
                                       fmc16_link_status_t *value);
fmc16_result_t fmc16_parse_status_snapshot(const uint16_t *packet, size_t words,
                                           fmc16_status_snapshot_t *value);
fmc16_result_t fmc16_parse_io_snapshot(const uint16_t *packet, size_t words,
                                       fmc16_io_snapshot_t *value);
fmc16_result_t fmc16_parse_mode_snapshot(const uint16_t *packet, size_t words,
                                         fmc16_mode_snapshot_t *value);
fmc16_result_t fmc16_parse_event(const uint16_t *packet, size_t words,
                                 fmc16_event_t *value);
fmc16_result_t fmc16_parse_response(const uint16_t *packet, size_t words,
                                    fmc16_response_t *value);
fmc16_result_t fmc16_parse_capability_response(const uint16_t *packet, size_t words,
                                               fmc16_capability_report_t *value);
fmc16_result_t fmc16_parse_link_status_response(const uint16_t *packet, size_t words,
                                                fmc16_link_status_t *value);
fmc16_result_t fmc16_parse_status_snapshot_response(
    const uint16_t *packet, size_t words, fmc16_status_snapshot_t *value);
fmc16_result_t fmc16_parse_io_control_response(const uint16_t *packet, size_t words,
                                               fmc16_io_control_result_t *value);
fmc16_result_t fmc16_parse_ack(const uint16_t *packet, size_t words,
                               fmc16_ack_t *value);

fmc16_result_t fmc16_build_get_link_status(uint16_t *packet, size_t capacity,
                                           uint32_t sequence, uint32_t transaction,
                                           uint8_t flags, size_t *words);
fmc16_result_t fmc16_build_get_status_snapshot(uint16_t *packet, size_t capacity,
                                               uint32_t sequence, uint32_t transaction,
                                               uint8_t flags, size_t *words);
fmc16_result_t fmc16_build_set_publish_period(uint16_t *packet, size_t capacity,
                                              uint32_t sequence, uint32_t transaction,
                                              uint8_t flags, uint16_t message_type,
                                              uint32_t period_ms, uint16_t threshold,
                                              size_t *words);
fmc16_result_t fmc16_build_subscribe_events(uint16_t *packet, size_t capacity,
                                            uint32_t sequence, uint32_t transaction,
                                            uint8_t flags, uint32_t event_mask,
                                            uint32_t min_interval_ms, size_t *words);
fmc16_result_t fmc16_build_unsubscribe_events(uint16_t *packet, size_t capacity,
                                              uint32_t sequence, uint32_t transaction,
                                              uint8_t flags, uint32_t event_mask,
                                              size_t *words);
fmc16_result_t fmc16_build_link_resync(uint16_t *packet, size_t capacity,
                                       uint32_t sequence, uint32_t transaction,
                                       uint8_t flags, uint16_t reason,
                                       uint32_t last_good_sequence, size_t *words);
fmc16_result_t fmc16_build_io_control_pi(uint16_t *packet, size_t capacity,
                                         uint32_t sequence, uint32_t transaction,
                                         uint8_t flags, uint16_t timeout_ms,
                                         uint16_t pi_update_mask, uint16_t pi_value,
                                         uint32_t expected_generation,
                                         uint32_t new_generation, size_t *words);
fmc16_result_t fmc16_build_host_status(uint16_t *packet, size_t capacity,
                                       uint32_t sequence,
                                       const fmc16_host_status_t *value,
                                       size_t *words);

#endif
