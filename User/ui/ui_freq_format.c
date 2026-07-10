/**
 * @file ui_freq_format.c
 * @brief 外部测频/周期字符串格式化实现。
 */

#include "ui_freq_format.h"
#include "dso/dso_freq_meter.h"

#include <stdio.h>

/** 无效/等待/故障态统一占位符（... / -- / ERR） */
static void write_text(char *buf, size_t len, const char *text)
{
    if ((buf == NULL) || (len == 0u)) {
        return;
    }
    (void)snprintf(buf, len, "%s", text);
}

/*
*********************************************************************************************************
*   函 数 名: write_invalid_by_state
*   功能说明: 在 UI 模块中完成页面显示、用户输入、波形绘制、状态同步或性能诊断处理。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static void write_invalid_by_state(bool valid, uint8_t state, char *buf, size_t len)
{
    if (valid) {
        write_text(buf, len, "--");
        return;
    }

    switch ((DSO_FreqMeterState)state) {
    case DSO_FREQ_METER_STATE_WAIT_EDGE:
    case DSO_FREQ_METER_STATE_MEASURING:
        write_text(buf, len, "...");
        break;
    case DSO_FREQ_METER_STATE_OVERRANGE:
    case DSO_FREQ_METER_STATE_FAULT:
        write_text(buf, len, "ERR");
        break;
    case DSO_FREQ_METER_STATE_STOPPED:
    case DSO_FREQ_METER_STATE_NO_SIGNAL:
    default:
        write_text(buf, len, "--");
        break;
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_FreqFormat_ExtMilliHz
*   功能说明: 格式化外部频率计频率、周期、模式或折叠显示文本。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_FreqFormat_ExtMilliHz(uint64_t freq_millihz,
                              bool valid,
                              uint8_t state,
                              char *buf,
                              size_t len)
{
    if ((buf == NULL) || (len == 0u)) {
        return;
    }

    if (!valid || (freq_millihz == 0ull)) {
        write_invalid_by_state(valid, state, buf, len);
        return;
    }

    if (freq_millihz >= 1000000000ull) {
        uint64_t whole = freq_millihz / 1000000000ull;
        uint64_t frac = (freq_millihz % 1000000000ull) / 1000ull;
        (void)snprintf(buf, len, "%llu.%06lluMHz",
                       (unsigned long long)whole,
                       (unsigned long long)frac);
    } else if (freq_millihz >= 1000000ull) {
        uint64_t whole = freq_millihz / 1000000ull;
        uint64_t frac = (freq_millihz % 1000000ull) / 1000ull;
        (void)snprintf(buf, len, "%llu.%03llukHz",
                       (unsigned long long)whole,
                       (unsigned long long)frac);
    } else {
        uint64_t whole = freq_millihz / 1000ull;
        uint64_t frac = freq_millihz % 1000ull;
        (void)snprintf(buf, len, "%llu.%03lluHz",
                       (unsigned long long)whole,
                       (unsigned long long)frac);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_FreqFormat_ExtMilliHzCompact
*   功能说明: 格式化外部频率计频率、周期、模式或折叠显示文本。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_FreqFormat_ExtMilliHzCompact(uint64_t freq_millihz,
                                     bool valid,
                                     uint8_t state,
                                     char *buf,
                                     size_t len)
{
    if ((buf == NULL) || (len == 0u)) {
        return;
    }

    if (!valid || (freq_millihz == 0ull)) {
        write_invalid_by_state(valid, state, buf, len);
        return;
    }

    {
        uint64_t hz = (freq_millihz + 500ull) / 1000ull;

        (void)snprintf(buf,
                       len,
                       "%llu.%06lluMHz",
                       (unsigned long long)(hz / 1000000ull),
                       (unsigned long long)(hz % 1000000ull));
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_FreqFormat_ExtPeriod
*   功能说明: 格式化外部频率计频率、周期、模式或折叠显示文本。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_FreqFormat_ExtPeriod(uint64_t freq_millihz,
                             bool valid,
                             uint8_t state,
                             char *buf,
                             size_t len)
{
    if ((buf == NULL) || (len == 0u)) {
        return;
    }

    if (!valid || (freq_millihz == 0ull)) {
        write_invalid_by_state(valid, state, buf, len);
        return;
    }

    if (freq_millihz >= 1000000000ull) {
        uint64_t ns_x1000 = (1000000000000000ull + (freq_millihz / 2ull)) / freq_millihz;
        (void)snprintf(buf, len, "%llu.%03lluns",
                       (unsigned long long)(ns_x1000 / 1000ull),
                       (unsigned long long)(ns_x1000 % 1000ull));
    } else if (freq_millihz >= 1000000ull) {
        uint64_t us_x1000 = (1000000000000ull + (freq_millihz / 2ull)) / freq_millihz;
        (void)snprintf(buf, len, "%llu.%03lluus",
                       (unsigned long long)(us_x1000 / 1000ull),
                       (unsigned long long)(us_x1000 % 1000ull));
    } else {
        uint64_t ms_x1000 = (1000000000ull + (freq_millihz / 2ull)) / freq_millihz;
        (void)snprintf(buf, len, "%llu.%03llums",
                       (unsigned long long)(ms_x1000 / 1000ull),
                       (unsigned long long)(ms_x1000 % 1000ull));
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_FreqFormat_ExtMode
*   功能说明: 格式化外部频率计频率、周期、模式或折叠显示文本。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_FreqFormat_ExtMode(uint8_t state,
                           uint8_t mode,
                           char *buf,
                           size_t len)
{
    if ((buf == NULL) || (len == 0u)) {
        return;
    }

    switch ((DSO_FreqMeterState)state) {
    case DSO_FREQ_METER_STATE_VALID:
        if ((DSO_FreqMeterMode)mode == DSO_FREQ_METER_MODE_LOW_CAPTURE) {
            write_text(buf, len, "CAP");
        } else {
            write_text(buf, len, "GATE");
        }
        break;
    case DSO_FREQ_METER_STATE_WAIT_EDGE:
    case DSO_FREQ_METER_STATE_MEASURING:
        write_text(buf, len, "MEAS");
        break;
    case DSO_FREQ_METER_STATE_NO_SIGNAL:
        write_text(buf, len, "NO SIG");
        break;
    case DSO_FREQ_METER_STATE_OVERRANGE:
        write_text(buf, len, "OVR");
        break;
    case DSO_FREQ_METER_STATE_FAULT:
        write_text(buf, len, "ERR");
        break;
    case DSO_FREQ_METER_STATE_STOPPED:
    default:
        write_text(buf, len, "--");
        break;
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_FreqFormat_FoldMilliHz
*   功能说明: 格式化外部频率计频率、周期、模式或折叠显示文本。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t UI_FreqFormat_FoldMilliHz(uint64_t value)
{
    return (uint32_t)value ^ (uint32_t)(value >> 32);
}
