/**
 * @file bsp_ui_io_expander.c
 * @brief MCP23017 面板 IO 扩展器读写（软件 I2C 走共享 bsp_i2c_ui 总线）。
 */

/*
 * MCP23017 @ I2C 地址 0x26（7-bit），PB8=SCL、PB7=SDA，GPIO 模拟 I2C（开漏 + 外部上拉）。
 * 与 PCAL6524(0x20) 共享 PB7/PB8 总线，访问由 bsp_i2c_ui 的互斥锁串行化。
 */

#include "bsp.h"
#include "bsp_ui_io_expander.h"
#include "bsp_i2c_ui.h"

/* 宏定义: UI_I2C_ADDR7 - MCP23017 7-bit 地址。 */
#define UI_I2C_ADDR7    0x26u
/* 宏定义: MCP23017_REG_IODIRA - IODIRA 寄存器。 */
#define MCP23017_REG_IODIRA  0x00u
/* 宏定义: MCP23017_REG_IODIRB - IODIRB 寄存器。 */
#define MCP23017_REG_IODIRB  0x01u
/* 宏定义: MCP23017_REG_GPIOB - MCP23017_REG_GPIOB 的宏定义。 */
#define MCP23017_REG_GPIOB  0x13u

/*
*********************************************************************************************************
*   函 数 名: BSP_UI_IO_Init
*   功能说明: 初始化 MCP23017 方向寄存器（A 口输出接 LED，B 口输入接编码器/按键）。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void BSP_UI_IO_Init(void)
{
    BSP_I2C_UI_Init();
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WriteReg(UI_I2C_ADDR7, MCP23017_REG_IODIRA, 0x00u); /* IODIRA=0 输出(接 LED) */
    (void)BSP_I2C_UI_WriteReg(UI_I2C_ADDR7, MCP23017_REG_IODIRB, 0xFFu); /* IODIRB=0xFF 输入(编码器) */
    BSP_I2C_UI_Unlock();
}

/*
*********************************************************************************************************
*   函 数 名: BSP_UI_IO_ReadPortB
*   功能说明: 读取 MCP23017 GPIOB 寄存器（0x13）。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int BSP_UI_IO_ReadPortB(uint8_t *out)
{
    int rc;

    if (out == NULL) {
        return -1;
    }

    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_ReadReg(UI_I2C_ADDR7, MCP23017_REG_GPIOB, out);
    BSP_I2C_UI_Unlock();
    return rc;
}
