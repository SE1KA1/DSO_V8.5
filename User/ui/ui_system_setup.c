/**
 * @file ui_system_setup.c
 * @brief 系统页状态与 8-bit(3L) / 10-bit 采集格式切换逻辑。
 */
#include "ui_system_setup.h"

#include "dso_integration.h"
#include "dso/dso_acq.h"
#include "dso/dso_config.h"
#include "dso/dso_sample_spec.h"
#include "ui_nav.h"

#include <stdio.h>

static uint8_t s_focus_row;

/*
*********************************************************************************************************
*   函 数 名: UI_System_OnScreenShown
*   功能说明: 维护系统设置页面的采样格式、校准、显示和系统参数。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_System_OnScreenShown(void)
{
    s_focus_row = 0u;
}

/*
*********************************************************************************************************
*   函 数 名: UI_System_GetFocusRow
*   功能说明: 维护系统设置页面的采样格式、校准、显示和系统参数。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t UI_System_GetFocusRow(void)
{
    if (s_focus_row > 1u) {
        s_focus_row = 1u;
    }
    return s_focus_row;
}

/*
*********************************************************************************************************
*   函 数 名: UI_System_Ec1Detent
*   功能说明: 维护系统设置页面的采样格式、校准、显示和系统参数。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_System_Ec1Detent(int dir)
{
    if (dir > 0) {
        if (s_focus_row < 1u) {
            s_focus_row++;
        }
    } else if (dir < 0) {
        if (s_focus_row > 0u) {
            s_focus_row--;
        }
    }
    UI_Nav_MarkDirty();
}

/*
*********************************************************************************************************
*   函 数 名: UI_System_Ec2Detent
*   功能说明: 维护系统设置页面的采样格式、校准、显示和系统参数。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_System_Ec2Detent(int dir)
{
    DSO_SampleFormat cur_fmt;
    DSO_SampleFormat next_fmt;
    uint8_t          cur_topo;
    uint8_t          next_topo;

    if (s_focus_row != 0u || dir == 0) {
        return;
    }

    cur_fmt  = DSO_Acq_GetPendingSampleFormat();
    cur_topo = DSO_Acq_GetPendingHs8Topology();
    next_fmt = cur_fmt;
    (void)cur_topo;
    next_topo = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;

    if (dir > 0) {
        if (cur_fmt == DSO_SAMPLE_FORMAT_10BIT) {
            next_fmt  = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
        } else {
            next_fmt  = DSO_SAMPLE_FORMAT_10BIT;
        }
    } else {
        if (cur_fmt == DSO_SAMPLE_FORMAT_10BIT) {
            next_fmt  = DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED;
        } else {
            next_fmt  = DSO_SAMPLE_FORMAT_10BIT;
        }
    }

    DSO_Acq_SetPendingHs8Topology(next_topo);
    DSO_Acq_SetSampleFormat(next_fmt);

#if (USE_DSO_ENGINE == 0)
    DSO_SampleSpec_SetActive(next_fmt);
#else
    if (next_fmt == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED &&
        DSO_Acq_GetSampleFormat() == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) {
        (void)DSO_Acq_RequestHs8Topology(next_topo);
    }
#endif
    UI_Nav_MarkDirty();
}

/*
*********************************************************************************************************
*   函 数 名: UI_System_AdcFormatLabel
*   功能说明: 维护系统设置页面的采样格式、校准、显示和系统参数。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
const char *UI_System_AdcFormatLabel(void)
{
    static char buf[32];
    DSO_SampleFormat pend = DSO_Acq_GetPendingSampleFormat();
    uint8_t          pto  = DSO_ACQ8_TOPOLOGY_TRI_INTERLEAVED;
    const char      *base;

    if (pend == DSO_SAMPLE_FORMAT_10BIT) {
        base = "10-bit";
    } else {
        base = "8-bit (3L)";
    }

#if (USE_DSO_ENGINE == 0)
    {
        DSO_SampleFormat act = DSO_SampleSpec_GetActiveFormat();
        int              star;

        star = (pend != act) ? 1 : 0;
        if (star) {
            (void)snprintf(buf, sizeof(buf), "%s *", base);
        } else {
            (void)snprintf(buf, sizeof(buf), "%s", base);
        }
    }
#else
    {
        DSO_SampleFormat act = DSO_Acq_GetSampleFormat();
        int              star;

        star = (pend != act) ? 1 : 0;
        if (!star && (pend == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED) &&
            (act == DSO_SAMPLE_FORMAT_8BIT_HIGH_SPEED)) {
            star = (DSO_Acq_GetHs8Topology() != pto) ? 1 : 0;
        }
        if (star) {
            (void)snprintf(buf, sizeof(buf), "%s *", base);
        } else {
            (void)snprintf(buf, sizeof(buf), "%s", base);
        }
    }
#endif
    return buf;
}
