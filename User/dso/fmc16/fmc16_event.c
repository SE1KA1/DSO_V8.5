/**
 * @file fmc16_event.c
 * @brief 事件解析实现。
 */
#include "fmc16_event.h"
#include <string.h>

int fmc16_event_parse(const uint16_t *payload, uint16_t payload_words,
                      Fmc16Event *out)
{
    if (!payload || payload_words < 7 || !out) return 0;
    memset(out, 0, sizeof(*out));

    out->code           = payload[0];
    out->source         = payload[1];
    out->timestamp_ms   = FMC16_U32(payload[2], payload[3]);
    out->generation     = FMC16_U32(payload[4], payload[5]);
    out->schema_version = payload[6];

    /* 事件数据: P7 起 */
    if (payload_words > 7)
    {
        out->data_words = (payload_words - 7u > 8u) ? 8u : (payload_words - 7u);
        memcpy(out->data, &payload[7], out->data_words * sizeof(uint16_t));
    }

    return 1;
}
