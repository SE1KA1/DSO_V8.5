/**
 * @file bsp_i2c_eeprom_24xx_keil.c
 * @brief 安富莱 BSP 24xx EEPROM Keil 专用实现；DSO 主链路未启用。
 */

#include "bsp.h"
#include "bsp_i2c_eeprom_24xx.h"
#include "bsp_i2c_gpio.h"

/*
*********************************************************************************************************
*   函 数 名: ee_send_address
*   功能说明: ee_send_address 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t ee_send_address(uint16_t address)
{
    if (EE_ADDR_BYTES == 1u) {
        i2c_SendByte((uint8_t)address);
        if (i2c_WaitAck() != 0u) {
            return 0u;
        }
    } else {
        i2c_SendByte((uint8_t)(address >> 8));
        if (i2c_WaitAck() != 0u) {
            return 0u;
        }

        i2c_SendByte((uint8_t)address);
        if (i2c_WaitAck() != 0u) {
            return 0u;
        }
    }

    return 1u;
}

/*
*********************************************************************************************************
*   函 数 名: ee_wait_ready
*   功能说明: ee_wait_ready 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static uint8_t ee_wait_ready(void)
{
    uint16_t retry;

    for (retry = 0u; retry < 2000u; ++retry) {
        i2c_Start();
        i2c_SendByte(EE_DEV_ADDR | I2C_WR);
        if (i2c_WaitAck() == 0u) {
            i2c_Stop();
            return 1u;
        }
        i2c_Stop();
    }

    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: ee_CheckOk
*   功能说明: ee_CheckOk 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t ee_CheckOk(void)
{
    if (i2c_CheckDevice(EE_DEV_ADDR) == 0u) {
        return 1u;
    }

    i2c_Stop();
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: ee_ReadBytes
*   功能说明: ee_ReadBytes 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t ee_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize)
{
    uint16_t i;

    if ((_pReadBuf == NULL) || (_usSize == 0u)) {
        return 1u;
    }

    i2c_Start();
    i2c_SendByte(EE_DEV_ADDR | I2C_WR);
    if (i2c_WaitAck() != 0u) {
        goto cmd_fail;
    }

    if (ee_send_address(_usAddress) == 0u) {
        goto cmd_fail;
    }

    i2c_Start();
    i2c_SendByte(EE_DEV_ADDR | I2C_RD);
    if (i2c_WaitAck() != 0u) {
        goto cmd_fail;
    }

    for (i = 0u; i < _usSize; ++i) {
        _pReadBuf[i] = i2c_ReadByte();
        if (i + 1u < _usSize) {
            i2c_Ack();
        } else {
            i2c_NAck();
        }
    }

    i2c_Stop();
    return 1u;

cmd_fail:
    i2c_Stop();
    return 0u;
}

/*
*********************************************************************************************************
*   函 数 名: ee_WriteBytes
*   功能说明: ee_WriteBytes 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t ee_WriteBytes(uint8_t *_pWriteBuf, uint16_t _usAddress, uint16_t _usSize)
{
    uint16_t remaining = _usSize;
    uint16_t address = _usAddress;
    uint8_t *data = _pWriteBuf;

    if ((_pWriteBuf == NULL) || (_usSize == 0u)) {
        return 1u;
    }

    while (remaining > 0u) {
        uint16_t page_offset = (uint16_t)(address & (EE_PAGE_SIZE - 1u));
        uint16_t chunk = (uint16_t)(EE_PAGE_SIZE - page_offset);
        uint16_t i;

        if (chunk > remaining) {
            chunk = remaining;
        }

        i2c_Start();
        i2c_SendByte(EE_DEV_ADDR | I2C_WR);
        if (i2c_WaitAck() != 0u) {
            goto cmd_fail;
        }

        if (ee_send_address(address) == 0u) {
            goto cmd_fail;
        }

        for (i = 0u; i < chunk; ++i) {
            i2c_SendByte(data[i]);
            if (i2c_WaitAck() != 0u) {
                goto cmd_fail;
            }
        }

        i2c_Stop();
        if (ee_wait_ready() == 0u) {
            return 0u;
        }

        address = (uint16_t)(address + chunk);
        data += chunk;
        remaining = (uint16_t)(remaining - chunk);
    }

    return 1u;

cmd_fail:
    i2c_Stop();
    return 0u;
}
