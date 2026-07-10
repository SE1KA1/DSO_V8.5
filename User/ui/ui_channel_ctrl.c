/**
 * @file ui_channel_ctrl.c
 * @brief 逻辑通道 CH1/CH2 显示开关（默认 CH1=ON、CH2=OFF）。
 *
 * MainTask 绘制波形前查询 GetEnabled，与 Solo 模式组合决定格区可见性。
 * OnEnableChanged 为 weak 钩子，可强符号接入继电器/采集 mute 等。
 */
#include "ui_channel_ctrl.h"

/* 默认：CH1 开、CH2 关（与通道页初始示意一致） */
static uint8_t s_ch_en[2] = { 1u, 0u };

/*
*********************************************************************************************************
*   函 数 名: UI_ChannelCtrl_GetEnabled
*   功能说明: 维护通道设置页面的档位、耦合、探头、偏移和显示状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
bool UI_ChannelCtrl_GetEnabled(DSO_ChannelId ch)
{
    if (ch == DSO_CHANNEL_1) {
        return (s_ch_en[0] != 0u);
    }
    if (ch == DSO_CHANNEL_2) {
        return (s_ch_en[1] != 0u);
    }
    return true;
}

/*
*********************************************************************************************************
*   函 数 名: UI_ChannelCtrl_SetEnabled
*   功能说明: 维护通道设置页面的档位、耦合、探头、偏移和显示状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void UI_ChannelCtrl_SetEnabled(DSO_ChannelId ch, bool on)
{
    uint8_t v = on ? 1u : 0u;

    if (ch == DSO_CHANNEL_1) {
        if (s_ch_en[0] == v) {
            return;
        }
        s_ch_en[0] = v;
        UI_ChannelCtrl_OnEnableChanged(DSO_CHANNEL_1, on);
    } else if (ch == DSO_CHANNEL_2) {
        if (s_ch_en[1] == v) {
            return;
        }
        s_ch_en[1] = v;
        UI_ChannelCtrl_OnEnableChanged(DSO_CHANNEL_2, on);
    }
}

/*
*********************************************************************************************************
*   函 数 名: UI_ChannelCtrl_OnEnableChanged
*   功能说明: 维护通道设置页面的档位、耦合、探头、偏移和显示状态。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
__weak void UI_ChannelCtrl_OnEnableChanged(DSO_ChannelId ch, bool on)
{
    (void)ch;
    (void)on;
}
