/**
 * @file fmc16_event.h
 * @brief 异步事件处理: EVENT 包解析。
 */
#ifndef FMC16_EVENT_H
#define FMC16_EVENT_H

#include "fmc16.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 解码后的事件 ---- */
typedef struct {
    uint16_t code;
    uint16_t source;
    uint32_t timestamp_ms;
    uint32_t generation;
    uint16_t schema_version;
    uint16_t data[8];       /**< 事件数据 (最多 8 words) */
    uint16_t data_words;
} Fmc16Event;

/* ---- API ---- */

/** 解析 EVENT payload */
int fmc16_event_parse(const uint16_t *payload, uint16_t payload_words,
                      Fmc16Event *out);

#ifdef __cplusplus
}
#endif
#endif /* FMC16_EVENT_H */
