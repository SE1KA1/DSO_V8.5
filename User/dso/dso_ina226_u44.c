/**
 * @file dso_ina226_u44.c
 * @brief 核心板 U44-INA226 驱动（0x40/20mΩ），结构同 dso_ina226.c（U10 0x41/100mΩ）。
 *
 * 配置：Config 0x0527；Alert Limit = DSO_INA226U44_ALERT_LSB(800)；M/E = 0x8001（SOL + LEN 锁存）。
 * 判警：纯分流阈值分级（AFF 只进 power_ok 不强制等级）；恢复需连续 3 周期 ≤ 恢复线。
 * 每帧独立持锁、等待在锁外；prio3 防 GUI_Init spin 饿死。
 */
#include "dso_ina226_u44.h"
#include "bsp_i2c_ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define U44_RETRY_MS   1u

static volatile int32_t s_shunt_lsb = 0;
static volatile uint8_t s_power_ok = 1;
static volatile uint8_t s_level = 0;   /* 分级：0正常 1预警 2告警 3紧急 */
static uint8_t s_configured = 0;
static uint8_t s_normal_count = 0;
static uint16_t s_fail_streak = 0;
static uint8_t s_reported_first = 0;
static uint8_t s_reported_over = 0;

static int u44_wreg16(uint8_t reg, uint16_t val)
{
    uint8_t b[3];
    b[0] = reg;
    b[1] = (uint8_t)(val >> 8);
    b[2] = (uint8_t)(val & 0xFFu);
    return BSP_I2C_UI_WriteRaw(DSO_INA226U44_I2C_ADDR, b, 3u);
}

static int u44_rreg16(uint8_t reg, uint16_t *val)
{
    uint8_t p = reg;
    uint8_t d[2];
    if (val == NULL) return -1;
    if (BSP_I2C_UI_WriteRaw(DSO_INA226U44_I2C_ADDR, &p, 1u) != 0) return -1;
    if (BSP_I2C_UI_ReadRaw(DSO_INA226U44_I2C_ADDR, d, 2u) != 0) return -1;
    *val = (uint16_t)((d[0] << 8) | d[1]);
    return 0;
}

static int u44_read_alert_flag(uint8_t *aff)
{
    uint16_t me = 0;
    if (u44_rreg16(0x06u, &me) != 0) return -1;   /* 读 M/E 亦清锁存 */
    *aff = (me & 0x0010u) ? 1u : 0u;
    return 0;
}

static int u44_read_shunt(uint16_t *v)
{
    return u44_rreg16(0x01u, v);
}

static void u44_measure_once(void)
{
    uint16_t shunt = 0;
    uint8_t aff = 0u;
    int rc;
    int32_t lsb;
    uint8_t alarm;

    if (!s_configured) {
        BSP_I2C_UI_Lock();
        rc  = u44_wreg16(0x00u, 0x0527u);
        rc |= u44_wreg16(0x07u, (uint16_t)DSO_INA226U44_ALERT_LSB);
        rc |= u44_wreg16(0x06u, 0x8001u);   /* SOL + LEN 锁存 */
        BSP_I2C_UI_Unlock();
        if (rc != 0) {
            ++s_fail_streak;
            return;
        }
        s_configured = 1u;
    }

    BSP_I2C_UI_Lock();
    rc = u44_read_alert_flag(&aff);
    if (rc == 0) {
        rc = u44_read_shunt(&shunt);
    }
    BSP_I2C_UI_Unlock();
    if (rc != 0) {
        if (++s_fail_streak == 10u) {
            printf("[INA226U44] read fail streak=%u\r\n", (unsigned)s_fail_streak);
        }
        return;
    }
    s_fail_streak = 0u;

    lsb = (int32_t)(int16_t)shunt;
    s_shunt_lsb = lsb;
    /* 分级判警（纯分流阈值，四级，与 U10 一致）；AFF 只作"曾触发"参考不强制
     * 等级（芯片锁存可能残留，真实等级一律按当前分流值定）。
     * 恢复需连续 3 周期正常（软件锁存防抖）。 */
    alarm = (aff != 0u) || (lsb > (int32_t)DSO_INA226U44_ALERT_LSB);
    if (lsb > (int32_t)DSO_INA226U44_EMERG_LSB) {
        s_level = 3u;
    } else if (lsb > (int32_t)DSO_INA226U44_ALERT_LSB) {
        s_level = 2u;
    } else if (lsb > (int32_t)DSO_INA226U44_WARN_LSB) {
        s_level = 1u;
    } else {
        s_level = 0u;
    }
    if (alarm) {
        s_power_ok = 0u;
        s_normal_count = 0u;
    } else if (lsb <= (int32_t)DSO_INA226U44_RESTORE_LSB) {
        if (++s_normal_count >= 3u) {
            s_power_ok = 1u;
        }
    }

    if (!s_reported_first) {
        s_reported_first = 1u;
        printf("[INA226U44] first ok cur=%ldmA\r\n", (long)(lsb / 8));
    }
    if (!s_power_ok && !s_reported_over) {
        s_reported_over = 1u;
        printf("[INA226U44] OVER-CURRENT cur=%ldmA\r\n", (long)(lsb / 8));
    } else if (s_power_ok && s_reported_over) {
        s_reported_over = 0u;
        printf("[INA226U44] current normal again\r\n");
    }
}

static void u44_task(void *pv)
{
    (void)pv;
    printf("[INA226U44] task start (addr=0x%02X, R=20mOhm)\r\n",
           (unsigned)DSO_INA226U44_I2C_ADDR);
    for (;;) {
        u44_measure_once();
        vTaskDelay(pdMS_TO_TICKS(DSO_INA226U44_POLL_MS));
    }
}

void DSO_Ina226U44_Init(void)
{
    if (xTaskCreate(u44_task, "Ina44", 512, NULL, 3, NULL) != pdPASS) {
        printf("[BOOT] FAIL Ina44\r\n");
    }
}

uint8_t DSO_Ina226U44_PowerOk(void)
{
    return s_power_ok;
}

int32_t DSO_Ina226U44_GetCurMa(void)
{
    return s_shunt_lsb / 8;   /* 2.5µV/20mΩ → lsb*0.125 mA */
}

uint8_t DSO_Ina226U44_GetLevel(void)
{
    return s_level;
}
