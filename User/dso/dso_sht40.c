/**
 * @file dso_sht40.c
 * @brief SHT40-AD1B-R2 温度读取（软件 I2C BSP_I2C_UI，PB7/PB8，与 PCAL6524 同总线）。
 *
 * 协议：发 0xFD（高精度）→ 等 15ms → 读 6 字节 [Tmsb,Tlsb,Tcrc,RHmsb,RHlsb,RHcrc]。
 * 温度 = -45 + 175 * raw/65535（℃）；湿度 = -6 + 125 * raw/65535（%RH，钳位 0..100）。
 * CRC-8 poly 0x31 init 0xFF（Sensirion）。
 *
 * 总线互斥：命令与读取都持锁，但 15ms 等待在锁外（其他器件可用总线）。
 * 失败静默累计；从有效→失效（>10 连败）与首次成功各打印一次诊断，防刷屏。
 */
#include "dso_sht40.h"
#include "bsp_i2c_ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* 缓存：百分之一 ℃ / 百分之一 %RH；0 仅表示"尚未成功"（配合 s_valid 判读） */
static int32_t s_temp_centi = 0;
static int32_t s_rh_centi = 0;      /* 相对湿度 ×100（0..10000 = 0..100.00 %RH） */
static uint8_t s_valid = 0;       /* 1=最近有效 */
static uint8_t s_fail_streak = 0;
static uint8_t s_reported_invalid = 0;   /* 已打印过"失效"诊断（防刷屏） */
static uint8_t s_reported_first_ok = 0;

/* CRC-8（Sensirion）：poly 0x31，init 0xFF，MSB-first，无 xorout */
static uint8_t sht40_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFu;
    uint8_t i, j;
    for (i = 0u; i < len; i++) {
        crc ^= data[i];
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1) ^ 0x31u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* 单次测量；成功更新缓存并复位失败计数，失败累计 */
static void sht40_measure_once(void)
{
    static const uint8_t cmd = DSO_SHT40_CMD_MEASURE_HIGH;
    uint8_t buf[6];
    int rc;

    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_WriteRaw(DSO_SHT40_I2C_ADDR, &cmd, 1u);
    BSP_I2C_UI_Unlock();
    if (rc != 0) {
        goto fail;
    }

    /* 测量期间不持总线锁：SHT40 自持测量，其余器件仍可用总线 */
    vTaskDelay(pdMS_TO_TICKS(DSO_SHT40_MEASURE_DELAY_MS));

    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_ReadRaw(DSO_SHT40_I2C_ADDR, buf, 6u);
    BSP_I2C_UI_Unlock();
    if (rc != 0) {
        goto fail;
    }

    /* CRC 校验温度与湿度两组（每组 2 数据 + 1 CRC） */
    if (sht40_crc8(&buf[0], 2u) != buf[2] || sht40_crc8(&buf[3], 2u) != buf[5]) {
        goto fail;
    }

    {
        /* T[°C] = -45 + 175 * raw/65535；整数四舍五入到百分之一 ℃ */
        int64_t raw = (int64_t)buf[0] << 8 | buf[1];
        int32_t centi = -4500 + (int32_t)(((int64_t)17500 * raw + 32767) / 65535);
        /* RH[%] = -6 + 125 * raw/65535；四舍五入到百分之一 %，并按器件规格钳位 0..100% */
        int64_t raw_rh = (int64_t)buf[3] << 8 | buf[4];
        int32_t rh = -600 + (int32_t)(((int64_t)12500 * raw_rh + 32767) / 65535);

        if (rh < 0) {
            rh = 0;
        } else if (rh > 10000) {
            rh = 10000;
        }

        s_temp_centi = centi;
        s_rh_centi = rh;
        s_valid = 1u;
        s_fail_streak = 0u;
        if (!s_reported_first_ok) {
            s_reported_first_ok = 1u;
            printf("[SHT40] first ok temp=%d.%02dC rh=%d.%02d%%\r\n",
                   (int)(centi / 100), (int)(centi % 100),
                   (int)(rh / 100), (int)(rh % 100));
        }
    }
    return;

fail:
    if (++s_fail_streak > DSO_SHT40_FAIL_INVALID_N) {
        s_valid = 0u;
        if (!s_reported_invalid) {
            s_reported_invalid = 1u;
            printf("[SHT40] sensor lost (bus/addr?)\r\n");
        }
    }
}

static void DSO_Sht40_Task(void *pv)
{
    (void)pv;
    printf("[SHT40] task start (addr=0x%02X, alarm>%d.%02dC)\r\n",
           (unsigned)DSO_SHT40_I2C_ADDR,
           (int)(DSO_SHT40_ALARM_HI_CENTI / 100),
           (int)(DSO_SHT40_ALARM_HI_CENTI % 100));
    for (;;) {
        sht40_measure_once();
        vTaskDelay(pdMS_TO_TICKS(DSO_SHT40_POLL_MS));
    }
}

void DSO_Sht40_Init(void)
{
    /* 优先级 3（> MainTask 的 2）：GUI_Init 卡死时会 spin 饿死 prio-1 任务
     * （同 dso_switch_cmd 串口任务的处理）。1s 周期内仅占用 ~16ms，无碍。 */
    if (xTaskCreate(DSO_Sht40_Task, "SHT40", 512, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL SHT40\r\n");
    }
}

int32_t DSO_Sht40_GetTempDezi(void)
{
    /* 百分之一 → 十分之一（四舍五入） */
    return (s_temp_centi >= 0) ? ((s_temp_centi + 5) / 10) : ((s_temp_centi - 5) / 10);
}

uint8_t DSO_Sht40_IsValid(void)
{
    return s_valid;
}

int32_t DSO_Sht40_GetRhCenti(void)
{
    return s_rh_centi;
}

uint8_t DSO_Sht40_HiAlarm(void)
{
    return (s_valid != 0u && s_temp_centi > (int32_t)DSO_SHT40_ALARM_HI_CENTI) ? 1u : 0u;
}

uint8_t DSO_Sht40_GetLevel(void)
{
    if (s_valid == 0u) {
        return 0u;
    }
    if (s_temp_centi > (int32_t)DSO_SHT40_EMERG_CENTI) {
        return 3u;
    }
    if (s_temp_centi > (int32_t)DSO_SHT40_ALARM_HI_CENTI) {
        return 2u;
    }
    if (s_temp_centi > (int32_t)DSO_SHT40_WARN_CENTI) {
        return 1u;
    }
    return 0u;
}
