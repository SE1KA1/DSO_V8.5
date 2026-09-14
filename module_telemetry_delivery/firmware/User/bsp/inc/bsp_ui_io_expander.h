/**
 * @file bsp_ui_io_expander.h
 * @brief MCP23017 I2C IO 扩展：EC1/EC2 编码器 A/B/R 与按键。
 */

/**
 * MCP23017（面板按键/双 EC11）软件 I2C：PB8=SCL、PB7=SDA，器件地址 A2A1A0=110 → 7-bit 0x26。
 * 与触摸等模块的 PB6/PB9 模拟 I2C 独立，避免引脚冲突。
 */
#ifndef BSP_UI_IO_EXPANDER_H
/* 宏定义: BSP_UI_IO_EXPANDER_H - 头文件保护宏。 */
#define BSP_UI_IO_EXPANDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 GPIO 与 MCP23017（B 口输入，用于编码器） */
void BSP_UI_IO_Init(void);

/**
 * 读取 MCP23017 GPIOB 寄存器（0x13）。
 * @return 0 成功，-1 I2C/MCP 无应答
 */
int BSP_UI_IO_ReadPortB(uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BSP_UI_IO_EXPANDER_H */
