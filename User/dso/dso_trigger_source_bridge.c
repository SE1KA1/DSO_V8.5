/**
 * @file dso_trigger_source_bridge.c
 * @brief UI 触发源 → DSO_AcquireCtrl_SetTriggerSource 桥接。
 */

#include "dso/dso_acq.h"
#include "dso/dso_acquire_control.h"
#include "ui/ui_trigger_ui.h"

#include <stddef.h>
#include <string.h>

/*
*********************************************************************************************************
*   函 数 名: ui_source_from_acquire_info
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static UI_TrigUiSource_t ui_source_from_acquire_info(const DSO_AcquireCtrlInfo *info,
                                                     bool ch2_available)
{
    if ((info != NULL) &&
        (info->trigger_source == DSO_TRIGGER_SOURCE_CH2) &&
        ch2_available) {
        return UI_TRIG_SRC_CH2;
    }

    return UI_TRIG_SRC_CH1;
}

/*
*********************************************************************************************************
*   函 数 名: UI_TriggerCtrl_OnSourceChanged
*   功能说明: 根据所在模块职责完成状态查询、参数更新、数据处理或显示辅助逻辑。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_TriggerCtrl_OnSourceChanged(UI_TrigUiSource_t s)
{
    DSO_AcquireCtrlInfo info;
    bool ch2_available = DSO_Acq_IsCh2Available();

    memset(&info, 0, sizeof(info));

    if (s == UI_TRIG_SRC_CH2) {
        if (ch2_available) {
            DSO_AcquireCtrl_SetTriggerSource(DSO_TRIGGER_SOURCE_CH2);
            UI_TrigUi_SetSourceFromDso(UI_TRIG_SRC_CH2, 1);
        } else {
            DSO_AcquireCtrl_SetTriggerSource(DSO_TRIGGER_SOURCE_CH1);
            UI_TrigUi_SetSourceFromDso(UI_TRIG_SRC_CH1, 1);
        }
        return;
    }

    if (s == UI_TRIG_SRC_CH1) {
        DSO_AcquireCtrl_SetTriggerSource(DSO_TRIGGER_SOURCE_CH1);
        UI_TrigUi_SetSourceFromDso(UI_TRIG_SRC_CH1, 1);
        return;
    }

    DSO_AcquireCtrl_GetInfo(&info);
    UI_TrigUi_SetSourceFromDso(ui_source_from_acquire_info(&info, ch2_available), 1);
}
