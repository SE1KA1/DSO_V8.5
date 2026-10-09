/**
 * @file dso_atecc608b.c
 * @brief ATECC608B-SSHDA-T 驱动：I2C 命令/状态机（软件 I2C BSP_I2C_UI，PB7/PB8，7-bit 0x60）。
 *
 * 协议（Microchip DS40002138A/DS40002154B §4.1/§7）：
 *   写：START [addr+W] [word=0x03] {Count Op P1 P2lo P2hi Data… CRC16lo CRC16hi} STOP
 *   读：忙/空闲时设备对读地址 NACK → 轮询；就绪后读回输出组
 *        [Count][数据(count-3)…][CRC16lo][CRC16hi]（FIFO，可分多次读）
 *   无效组恒 Count=4：[4][状态][CRC16lo][CRC16hi]；状态 0x00=成功。
 *   CRC16：poly 0x8005（反射 0xA001），init 0x0000，LSB 先发。
 *
 * 唤醒：Sleep 中的芯片对**任何 I2C 地址一律 NACK**（看起来像总线上没有器件），
 * 必须先 Wake：SDA 在 SCL 空闲高时被拉低 ≥tWLO(60µs)，释放后上升沿唤醒，
 * 再等 ≥tWHI(1.5ms) 才能访问 7-bit 地址。两条实现：
 *   - GPIO 脉冲法（BSP_I2C_UI_WakePulseMeasUs，本文件主用）：不产生任何地址字节，
 *     不打扰同总线的 PCAL6524/SHT40/INA226；
 *   - 0x00 字节法（BSP_I2C_UI_WakeZeroBytes）：规范里的传统写法，但其第一个 0x00
 *     就是 I2C 通用呼叫地址（general call），本总线有器件会应答它，故该返回值
 *     **不能**当唤醒判据。
 * 关键约束：唤醒与寻址必须在**同一持锁窗口**内（中途只等 tWHI）——把锁放开会让
 * SHT40/INA226 的 1s 轮询插进窗口，探针也会落到唤醒窗口之外（曾因此恒判 NACK）。
 *
 * 每帧独立持锁（BSP_I2C_UI_Lock/Unlock）。
 *
 * 【档位与持锁的约定】BSP_I2C_UI_SetBitDelayUs() 影响**全总线**：必须
 * **先还原为 0 再 Unlock()**，否则同总线的 SHT40/INA226/PCAL 事务会被拖到慢档
 * （2026-10-09 用逻辑分析仪抓到过 1kHz 的 INA226 帧，根因就是解锁前没还原）。
 *
 * 【编码约定 —— 编译期坑，务必遵守】本文件是 UTF-8，但 Keil AC5 按 ANSI(GBK)
 * 解析源码：一个中文字符的末字节若紧邻 `"` 或 `\`，会与它凑成一个合法 GBK 汉字，
 * 把它吃掉 —— 表现是 `error: #8: missing closing quote`（或 `\r` 的 `\` 被吃掉）。
 * 因此**本文件的所有 printf 字符串一律只用 ASCII**（诊断输出在任意终端也干净）；
 * 中文只出现在注释里（注释里被吃掉一个字符无害）。新增打印请沿用 [ATECC-x] 前缀。
 */
#include "dso_atecc608b.h"
#include "bsp_i2c_ui.h"
#include "debug_uart_lock.h"
#include "stm32h7xx_hal.h"   /* HAL_GetTick（Arm 0 上电事实核对） */
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define ATECC_RETRY_MS   1u          /* 忙时轮询间隔 */
#define ATECC_PROBE_N    3u          /* 探测重试次数 */

/* Config 区关键字节偏移（ATECC608 存储器映射；注意 word n 的字节偏移 = n*4，
 * 故 LockValue/LockConfig 位于 word 0x15 的 byte2/byte3 = 0x56/0x57） */
#define ATECC_CFG_REV_OFF           0x08u   /* RevNum（608B 低字节 0x60） */
#define ATECC_CFG_SN8_OFF           0x0Cu   /* SN[8] */
#define ATECC_CFG_LOCK_VALUE_OFF    0x56u   /* LockValue（0x00=已锁） */
#define ATECC_CFG_LOCK_CONFIG_OFF   0x57u   /* LockConfig（0x00=已锁） */
#define ATECC_CFG_SLOT_LOCKED_OFF   0x58u   /* SlotLocked[0..1] */

static uint8_t s_last_status = 0xFFu;   /* 最近执行状态码 */
/* 当前工作地址：默认 0x60；探测发现实际配置地址（如 0x50/0x64）后更新 */
static uint8_t s_addr = DSO_ATECC608B_I2C_ADDR;

/* ---- CRC16（poly 0x8005 反射 0xA001，init 0，LSB 先发） ---- */

static uint16_t atecc_crc16(const uint8_t *d, uint16_t len)
{
    uint16_t crc = 0u;
    uint16_t i;
    uint8_t j;
    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)d[i];
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/* CRC 续算（供跨段拼接 [Count]+data 时使用） */
static uint16_t atecc_crc16_cont(const uint8_t *d, uint16_t len, uint16_t crc)
{
    uint16_t i;
    uint8_t j;
    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)d[i];
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

uint8_t DSO_Atecc608b_LastStatus(void)
{
    return s_last_status;
}

/* ---- 命令执行（组包 + 状态机轮询 + 响应校验） ---- */

/* 内部实现；对外的 DSO_Atecc608b_Exec 只是在本体外面开关 ACK 多点采样档 */
static int atecc_exec_body(uint8_t opcode, uint8_t p1, uint16_t p2,
                           const uint8_t *in, uint16_t in_len,
                           uint8_t *out, uint16_t out_max)
{
    uint8_t group[DSO_ATECC608B_GROUP_MAX];  /* 写命令组 */
    uint8_t body[DSO_ATECC608B_GROUP_MAX];   /* 读回数据+CRC 暂存 */
    uint8_t first;
    uint16_t count;
    uint16_t crc;
    uint16_t data_len;
    uint32_t deadline;
    int rc = 0;
    uint16_t i;

    s_last_status = 0xFFu;
    if ((in_len > 0u && in == NULL) || (out_max > 0u && out == NULL)) {
        return DSO_ATECC_ERR_PARAM;
    }

#if ATECC_WAKE_BEFORE_EXEC
    /* 芯片在 Sleep 时对任何命令都不响应 —— 每帧先按规范 Wake */
    if (DSO_Atecc608b_Wake(NULL, NULL) != 0) {
        return DSO_ATECC_ERR_TIMEOUT;
    }
#endif
    count = (uint16_t)(7u + in_len);          /* Count+Op+2×Param+Data+2×CRC */
    if (count < 7u || count > DSO_ATECC608B_GROUP_MAX) {
        return DSO_ATECC_ERR_PARAM;
    }

    group[0] = (uint8_t)count;
    group[1] = opcode;
    group[2] = p1;
    group[3] = (uint8_t)(p2 & 0xFFu);
    group[4] = (uint8_t)(p2 >> 8);
    for (i = 0u; i < in_len; i++) {
        group[5u + i] = in[i];
    }
    crc = atecc_crc16(group, count - 2u);
    group[count - 2u] = (uint8_t)(crc & 0xFFu);   /* LSB 先发 */
    group[count - 1u] = (uint8_t)(crc >> 8);

    /* 写：[word=0x03] + 命令组，单帧 */
    {
        uint8_t frame[DSO_ATECC608B_GROUP_MAX + 1u];
        frame[0] = DSO_ATECC608B_WORD_CMD;
        for (i = 0u; i < count; i++) {
            frame[1u + i] = group[i];
        }
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_WriteRaw(s_addr, frame,
                                 (uint8_t)(count + 1u));
        BSP_I2C_UI_Unlock();
    }
    if (rc != 0) {
        return DSO_ATECC_ERR_IO;
    }

    /* 状态机：忙时读地址 NACK → 1ms 重试，直到拿到 Count 或超时 */
    deadline = (uint32_t)xTaskGetTickCount() + pdMS_TO_TICKS(DSO_ATECC608B_RESP_MS);
    for (;;) {
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_ReadRaw(s_addr, &first, 1u);
        BSP_I2C_UI_Unlock();
        if (rc == 0) {
            break;
        }
        if ((int32_t)(deadline - (uint32_t)xTaskGetTickCount()) <= 0) {
            return DSO_ATECC_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(ATECC_RETRY_MS));
    }
    count = first;

    if (count == 4u) {
        /* 错误组：[4][status][crc_lo][crc_hi]（首字节已读，余 3 字节） */
        uint8_t rest[3];
        BSP_I2C_UI_Lock();
        rc = BSP_I2C_UI_ReadRaw(s_addr, rest, 3u);
        BSP_I2C_UI_Unlock();
        if (rc != 0) {
            return DSO_ATECC_ERR_IO;
        }
        crc = atecc_crc16(&first, 1u);            /* CRC 覆盖 [count][status] */
        crc = atecc_crc16_cont(&rest[0], 1u, crc);
        if ((uint8_t)(crc & 0xFFu) != rest[1] || (uint8_t)(crc >> 8) != rest[2]) {
            return DSO_ATECC_ERR_CRC;
        }
        s_last_status = rest[0];
        if (s_last_status != 0x00u) {
            return DSO_ATECC_ERR_EXEC;
        }
        return 0;                                 /* 成功且无输出 */
    }

    if (count < 5u || count > (uint16_t)(out_max + 3u)) {
        return DSO_ATECC_ERR_PARAM;               /* 输出超预期 */
    }
    data_len = (uint16_t)(count - 3u);
    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_ReadRaw(s_addr, body, (uint8_t)(count - 1u));
    BSP_I2C_UI_Unlock();
    if (rc != 0) {
        return DSO_ATECC_ERR_IO;
    }
    /* CRC 覆盖 [count]+data（共 count-2 字节）；尾两字节为收到的 CRC */
    crc = atecc_crc16(&first, 1u);
    crc = atecc_crc16_cont(body, data_len, crc);
    if ((uint8_t)(crc & 0xFFu) != body[data_len] ||
        (uint8_t)(crc >> 8) != body[data_len + 1u]) {
        return DSO_ATECC_ERR_CRC;
    }
    for (i = 0u; i < data_len; i++) {
        out[i] = body[i];
    }
    return (int)data_len;
}

int DSO_Atecc608b_Exec(uint8_t opcode, uint8_t p1, uint16_t p2,
                       const uint8_t *in, uint16_t in_len,
                       uint8_t *out, uint16_t out_max)
{
    int rc;
    uint32_t saved = BSP_I2C_UI_GetBitDelayUs();

    /* ATECC 会话内：换到会话档位延迟 + 用多点采样判 ACK。
     * 依据：示波器实测芯片在"第 9 个时钟 SCL 高电平期"拉低 SDA，而单点采样漏读
     * （把 ACK 判成 NACK）。30µs 半周期与 32 点采样正好铺满高电平期，数据相位也有余量。
     * 用完立刻还原档位与采样模式，保证 SHT40/INA226/PCAL 的时序路径完全不变。 */
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
    BSP_I2C_UI_SetAckMultiSample(1u);
    rc = atecc_exec_body(opcode, p1, p2, in, in_len, out, out_max);
    BSP_I2C_UI_SetAckMultiSample(0u);
    BSP_I2C_UI_SetBitDelayUs(saved);
    return rc;
}

/* ---- 唤醒（TNGTLS 规范） ---- */

static int atecc_readprobe(uint8_t addr);   /* 前置声明：供 Probe / 诊断使用 */

int DSO_Atecc608b_Wake(uint8_t *zero_acked, uint16_t *ready_ms)
{
    uint32_t t0;
    uint32_t el;
    uint8_t acked;
    int p = -1;

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        /* 调度器未启动：不能 vTaskDelay，只能 GPIO 脉冲唤醒 */
        BSP_I2C_UI_WakePulse();
        if (zero_acked != NULL) { *zero_acked = 0u; }
        if (ready_ms != NULL)   { *ready_ms = 0xFFFFu; }
        return DSO_ATECC_ERR_TIMEOUT;
    }

    /* 唤醒与寻址同处一个持锁窗口：中途释放锁会让同总线任务（SHT40/INA226 的 1s
     * 轮询）插进来，且探针可能落到唤醒窗口之外 → 只会看到恒 NACK。 */
    t0 = (uint32_t)xTaskGetTickCount();
    BSP_I2C_UI_Lock();
    acked = (uint8_t)BSP_I2C_UI_WakeZeroBytes(DSO_ATECC_WAKE_ZB_N);
    if (zero_acked != NULL) { *zero_acked = acked; }

    /* 规范：Wake 后 ≥tWHI(1.5ms) 才能访问；未中就按 tWHIST(≈20ms) 保守再等一轮 */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    p = BSP_I2C_UI_Probe(s_addr);
    if (p != 0) {
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
        p = BSP_I2C_UI_Probe(s_addr);
    }
    BSP_I2C_UI_Unlock();

    el = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    if (ready_ms != NULL) {
        *ready_ms = (p == 0) ? (uint16_t)el : 0xFFFFu;
    }
    return (p == 0) ? 0 : DSO_ATECC_ERR_TIMEOUT;
}

/* ---- 常用包装 ---- */

int DSO_Atecc608b_InfoRevision(uint8_t rev[4])
{
    int rc;
    if (rev == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    /* Info opcode 0x30，Param1=0x00(Revision)，无数据，回 4 字节版本 */
    rc = DSO_Atecc608b_Exec(ATECC_OP_INFO, 0x00u, 0x0000u,
                            NULL, 0u, rev, 4u);
    return (rc == 4) ? 0 : rc;
}

int DSO_Atecc608b_ReadConfig32(uint8_t block, uint8_t out[32])
{
    int rc;
    if (out == NULL) {
        return DSO_ATECC_ERR_PARAM;
    }
    rc = DSO_Atecc608b_Exec(ATECC_OP_READ, ATECC_READ32_CONFIG_BLOCK(block),
                            0x0000u, NULL, 0u, out, 32u);
    return (rc == 32) ? 0 : rc;
}

int DSO_Atecc608b_Probe(void)
{
    /* 候选地址：0x35 = ATECC608B-TNGTLS 出厂默认（权威）；0x36 备选；
     * 其余为历史候选。每个候选都先按规范 Wake，再"读探针 NACK 才发命令"
     * （读也 ACK → 内存映射型器件如 EEPROM，跳过以防误写）。 */
    static const uint8_t cand[] = { 0x36u, 0x60u, 0x35u, 0x50u, 0x64u, 0x68u };
    uint8_t rev[4];
    uint8_t ci;
    uint8_t n;
    int rc = DSO_ATECC_ERR_TIMEOUT;
    for (ci = 0u; ci < (uint8_t)(sizeof(cand) / sizeof(cand[0])); ci++) {
        s_addr = cand[ci];
        if (DSO_Atecc608b_Wake(NULL, NULL) != 0) {
            continue;   /* 未就绪（睡眠/无器件/该地址无应答） */
        }
        if (atecc_readprobe(cand[ci]) == 0) {
            continue;   /* 读也 ACK → 内存映射型（EEPROM 类），跳过，防误写 */
        }
        for (n = 0u; n < ATECC_PROBE_N; n++) {
            rc = DSO_Atecc608b_InfoRevision(rev);
            if (rc == 0) {
                return 0;   /* s_addr 已是找到的地址 */
            }
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
        }
    }
    return rc;
}

/* ---- 上电探测（一次性任务，打印后自删；无周期上报） ---- */

/* 只读探针：不带任何写、尝试读 1 字节。
 * 带内存映射的器件(EEPROM 等)任何时候 ACK 读；ATECC 空闲/无输出时读地址 NACK。 */
static int atecc_readprobe(uint8_t addr)
{
    uint8_t b;
    int rc;
    BSP_I2C_UI_Lock();
    rc = BSP_I2C_UI_ReadRaw(addr, &b, 1u);
    BSP_I2C_UI_Unlock();
    return rc;
}

/* 已知器件地址（不打标签跳过读探针）：PCAL6524 0x22、MCP23017 0x26/0x27、
 * INA226 0x40/0x41、SHT40 0x44、ATECC608B-TNGTLS 0x35/0x36 */
static int atecc_is_known(uint8_t addr)
{
    return (addr == 0x22u || addr == 0x26u || addr == 0x27u ||
            addr == 0x35u || addr == 0x36u ||
            addr == 0x40u || addr == 0x41u || addr == 0x44u);
}

/* 扫描 **0x01-0x7F** 收集应答地址（Arm E 用）—— 原从 0x08 起，会漏掉
 * CBUS(0x01)/保留段(0x02-0x07)；0x00（通用呼叫）由 Arm J 单独覆盖。
 * woke=0：冷态基线，完全不唤醒 —— 即"普通 i2cdetect 扫描"，
 *         规范明确：Sleep 中的芯片在这条基线上不可见。
 * woke=1：每 ATECC_SCAN_BATCH 个地址在**同一持锁窗口**内重新 GPIO 唤醒一次 + 等 tWHI，
 *         使芯片在整个扫描期间保持醒态 —— 这样即使它的地址不是候选值也会被逮到。
 * @param bit_us 位延迟（µs 半周期）——慢档用于排除"芯片 ACK 慢、主控采样过早误判 NACK" */
#define ATECC_SCAN_BATCH   8u

static uint8_t atecc_scan_collect(uint8_t *out, uint8_t out_max, uint8_t woke,
                                  uint32_t bit_us, uint8_t batch)
{
    uint8_t addr;
    uint8_t n = 0u;
    uint8_t in_batch = 0u;

    if (batch < 1u) {
        batch = 1u;
    }
    for (addr = 0x01u; addr <= 0x7Fu; addr++) {
        if (in_batch == 0u) {
            BSP_I2C_UI_SetBitDelayUs(bit_us);        /* 本批用该档 */
            BSP_I2C_UI_Lock();                       /* 分批持锁（不做整段占锁） */
            if (woke != 0u) {
                (void)BSP_I2C_UI_WakePulseMeasUs(100u);   /* 唤醒 + 本批探针同窗口 */
                vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            }
        }
        if (BSP_I2C_UI_Probe(addr) == 0) {
            if (n < out_max) {
                out[n] = addr;
            }
            n++;
        }
        in_batch++;
        if (in_batch >= batch) {
            in_batch = 0u;
            BSP_I2C_UI_SetBitDelayUs(0u);            /* 先还原档位再解锁 */
            BSP_I2C_UI_Unlock();
        }
    }
    if (in_batch != 0u) {
        BSP_I2C_UI_SetBitDelayUs(0u);
        BSP_I2C_UI_Unlock();
    }
    return (n > out_max) ? out_max : n;
}

/* 打印一个地址列表（(k)=atecc_is_known 已知器件） */
static void atecc_diag_addrs(const char *tag, const uint8_t *l, uint8_t n)
{
    uint8_t i;
    DebugUartLock();
    printf("%s(%u):", tag, (unsigned)n);
    for (i = 0u; i < n; i++) {
        printf(" 0x%02X", (unsigned)l[i]);
        if (atecc_is_known(l[i])) {
            printf("(k)");
        }
    }
    printf("\r\n");
    DebugUartUnlock();
}

/* 打印差集（只在醒态出现的地址 = ATECC 候选）。@return 差集个数 */
static uint8_t atecc_diag_diff(const char *tag, const uint8_t *cold, uint8_t nc,
                               const uint8_t *woke, uint8_t nw)
{
    uint8_t i;
    uint8_t j;
    uint8_t found;
    uint8_t n = 0u;

    DebugUartLock();
    printf("%s:", tag);
    for (i = 0u; i < nw; i++) {
        found = 0u;
        for (j = 0u; j < nc; j++) {
            if (woke[i] == cold[j]) {
                found = 1u;
                break;
            }
        }
        if (found == 0u) {
            printf(" 0x%02X", (unsigned)woke[i]);
            n++;
        }
    }
    if (n == 0u) {
        printf(" (none)");
    }
    printf("\r\n");
    DebugUartUnlock();
    return n;
}

/* 唤醒窗口内一次性探完「候选 + 同窗口控制器件」。
 * 核心修正：唤醒与寻址必须在**同一持锁窗口**内完成（中途只等 tWHI）——把锁放开会让
 * SHT40/INA226 的 1s 轮询插进窗口，探针也会落到唤醒窗口之外（曾因此恒判 NACK）。
 * @param method  0=GPIO 脉冲法（SCL 保持高、无地址字节）；1=0x00 字节法（general call）
 * @param out_ack 长度 ≥6：0x60/0x35/0x36 候选 + 0x22/0x44/0x41 控制器件 的 ACK 位
 * @return method=0：实测低电平 µs；method=1：general-call 应答位
 */
static uint32_t atecc_wake_window(uint8_t method, uint8_t out_ack[6])
{
    static const uint8_t cand[6] = { 0x60u, 0x35u, 0x36u,   /* 候选 */
                                     0x22u, 0x44u, 0x41u }; /* 同窗口控制器件 */
    uint32_t ret;
    uint8_t i;

    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);   /* ≤100kHz，用完立刻还原 */
    BSP_I2C_UI_Lock();
    if (method == 0u) {
        ret = BSP_I2C_UI_WakePulseMeasUs(100u);
    } else {
        ret = (uint32_t)BSP_I2C_UI_WakeZeroBytes(DSO_ATECC_WAKE_ZB_N);
    }
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));       /* 持锁等 tWHI（≥1.5ms） */
    for (i = 0u; i < 6u; i++) {
        out_ack[i] = (uint8_t)((BSP_I2C_UI_Probe(cand[i]) == 0) ? 1u : 0u);
    }
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    return ret;
}

/* Arm C 用：唤醒 → 等 wait_ms → 探 addr（同窗口持锁） */
static uint8_t atecc_probe_after_wait(uint8_t addr, uint16_t wait_ms)
{
    uint8_t a;

    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    a = (uint8_t)((BSP_I2C_UI_Probe(addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    return a;
}

/* Arm H 用：用 ATECC **自己的地址字节**做唤醒 —— CryptoAuthLib hal_i2c_wake 的写法：
 * 100kHz 下 START + [addr+W] + 0x00，靠地址字节/数据字节的连续 0 位凑出 ≥tWLO 的低电平。
 * 这是最后一种"文档记载的唤醒形态"（另一种是 GPIO 脉冲、以及 0x00 通用呼叫字节法）。
 * @param burst  0=单次；1=连发 10 次
 * @param addr_acked 输出：唤醒序列中**地址字节本身**是否被 ACK（醒着的芯片才会 ACK）
 * @return 1=唤醒后的地址探测被 ACK
 */
static uint8_t atecc_wake_own_addr(uint8_t addr, uint8_t burst,
                                   uint8_t *addr_acked)
{
    static const uint8_t z = 0x00u;
    uint8_t acklog[4];
    uint8_t aa = 0u;
    uint8_t n = (burst != 0u) ? 10u : 1u;
    uint8_t r;
    uint8_t i;

    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    BSP_I2C_UI_Lock();
    for (i = 0u; i < n; i++) {
        if (BSP_I2C_UI_DiagTxBytes(addr, &z, 1u, acklog) == 0) {
            aa = 1u;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    r = (uint8_t)((BSP_I2C_UI_Probe(addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    if (addr_acked != NULL) {
        *addr_acked = aa;
    }
    return r;
}

/* Arm D 用：指定位延迟（µs 半周期）下唤醒并探 addr（同窗口持锁） */
static uint8_t atecc_probe_at_rate(uint8_t addr, uint32_t bit_us)
{
    uint8_t a;

    BSP_I2C_UI_SetBitDelayUs(bit_us);
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    a = (uint8_t)((BSP_I2C_UI_Probe(addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    return a;
}

/* ---- 分层判定实验（Arm 0/A/B/H/G/C/D/E/F）：全程**只读** ----
 * 只用 地址探测 / Info / Read；无 Write、无 Lock、无 OTP（调试红线）。
 * 每臂只回答一个问题、输出一行，最后一行是总结论。
 * @return 探到的 ATECC 候选地址（0 = 未探到）
 */
static uint8_t atecc608b_diag_battery(void)
{
    static const uint8_t cand[3] = { 0x60u, 0x35u, 0x36u };
    static const uint16_t wms[3] = { 2u, 20u, 50u };      /* Arm C 等待档 */
    static const uint32_t bds[4] = { 5u, 10u, 30u, 60u }; /* Arm D 位延迟档 */
#if DSO_ATECC_DIAG_ARM_F
    static const uint32_t pws[5] = { 60u, 100u, 250u, 1000u, 5000u };
#endif
    uint8_t ack[6];
    uint8_t cold[16];
    uint8_t woke[16];
    uint8_t woke8[16];
    uint8_t nc = 0u;
    uint8_t nw = 0u;
    uint8_t nw8 = 0u;
    uint8_t nd = 0u;
    uint8_t nd8 = 0u;
    uint8_t r;
    uint8_t i;
    uint8_t a;
    uint8_t j00 = 0u;       /* Arm J：通用呼叫地址 0x00 是否给出 ATECC 型响应 */
    uint8_t hit = 0u;       /* 命中的候选地址 */
    uint8_t ctrl_ok = 0u;   /* 同窗口控制器件是否应答 */
    uint8_t sda;
    uint8_t scl;
    uint8_t sda2;
    uint8_t scl2;
    uint8_t sd3[3];
    uint8_t aa;
    uint32_t meas;

    /* ---- Arm 0：上电事实核对 + 冷态基线 ----
     * 顺序有讲究：冷态基线必须排在**任何可能唤醒芯片的动作之前**。
     * BSP_I2C_UI_SdaLowProbe() 会把 SDA 拉低 120µs（≥tWLO 60µs）——它本身就是一次
     * 唤醒脉冲，故只能排在冷态扫描之后；BusRecover 的 9 个 SCL 脉冲不含长低电平，
     * 无唤醒作用（只把可能卡低的从机放开）。 */
    BSP_I2C_UI_Lock();
    BSP_I2C_UI_IdleLevels(&sda, &scl);
    BSP_I2C_UI_Unlock();

    /* 冷态基线（Arm E 的对照项）：此时未发过任何唤醒脉冲 —— 即规范点名的
     * "上电延时后跑普通 i2cdetect 扫描"，Sleep 中的芯片对地址一律 NACK。 */
    nc = atecc_scan_collect(cold, (uint8_t)sizeof(cold), 0u,
                            DSO_ATECC_SCAN_FAST_US, 8u);
    atecc_diag_addrs("[ATECC-0 ] cold(no wake yet)", cold, nc);

    /* 这三项测量必须持锁：电池延后 1.2s 后 SHT40/INA226 的 1s 轮询已在跑，
     * 不持锁会被别的任务在同一根线上交错驱动，读出假值（曾读到 sda_drive=0，
     * 与 idle SDA=1 及同轮三个器件 ACK 自相矛盾）。sda_drive 连测 3 次便于识别偶发。 */
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_BusRecover(&sda2, &scl2);
    for (i = 0u; i < 3u; i++) {
        sd3[i] = (uint8_t)BSP_I2C_UI_SdaLowProbe();
    }
    BSP_I2C_UI_Unlock();
    DebugUartLock();
    printf("[ATECC-0 ] por=%ums(need>=20) idle SDA=%u SCL=%u | recover SDA=%u SCL=%u "
           "sda_drive=%u/%u/%u id_bit=%uus(ident rate)\r\n",
           (unsigned)HAL_GetTick(), (unsigned)sda, (unsigned)scl,
           (unsigned)sda2, (unsigned)scl2,
           (unsigned)sd3[0], (unsigned)sd3[1], (unsigned)sd3[2],
           (unsigned)DSO_ATECC_BIT_DELAY_US);
    DebugUartUnlock();

    /* ---- Arm A：GPIO 脉冲法 + 唤醒窗口内一次性探完候选与控制器件（核心修正） ---- */
    for (r = 0u; r < DSO_ATECC_DIAG_ROUNDS; r++) {
        meas = atecc_wake_window(0u, ack);
        for (i = 0u; i < 3u; i++) {
            if (ack[i] != 0u && hit == 0u) {
                hit = cand[i];
            }
        }
        if (ack[3] != 0u || ack[4] != 0u || ack[5] != 0u) {
            ctrl_ok = 1u;
        }
        DebugUartLock();
        printf("[ATECC-A%u] gpio-pulse meas=%uus(need>=60) 0x60=%s 0x35=%s 0x36=%s "
               "| ctrl 0x22=%s 0x44=%s 0x41=%s\r\n",
               (unsigned)(r + 1u), (unsigned)meas,
               ack[0] ? "ACK" : "NACK", ack[1] ? "ACK" : "NACK", ack[2] ? "ACK" : "NACK",
               ack[3] ? "ACK" : "NACK", ack[4] ? "ACK" : "NACK", ack[5] ? "ACK" : "NACK");
        DebugUartUnlock();
    }

    /* ---- Arm B：0x00 字节法（general call）对照 ---- */
    meas = atecc_wake_window(1u, ack);
    for (i = 0u; i < 3u; i++) {
        if (ack[i] != 0u && hit == 0u) {
            hit = cand[i];
        }
    }
    if (ack[3] != 0u || ack[4] != 0u || ack[5] != 0u) {
        ctrl_ok = 1u;
    }
    DebugUartLock();
    printf("[ATECC-B ] zb0x00x%u gc_ack=%u(general-call ack, NOT a wake indicator) "
           "0x60=%s 0x35=%s 0x36=%s | ctrl 0x22=%s 0x44=%s 0x41=%s\r\n",
           (unsigned)DSO_ATECC_WAKE_ZB_N, (unsigned)meas,
           ack[0] ? "ACK" : "NACK", ack[1] ? "ACK" : "NACK", ack[2] ? "ACK" : "NACK",
           ack[3] ? "ACK" : "NACK", ack[4] ? "ACK" : "NACK", ack[5] ? "ACK" : "NACK");
    DebugUartUnlock();

    /* ---- Arm H：用 ATECC 自己的地址字节唤醒（单次 / 连发 10 次）----
     * 地址字节 ACK 与否本身也是判据：醒着的芯片会 ACK 自己的地址。 */
    a = atecc_wake_own_addr(s_addr, 0u, &aa);
    ack[0] = a;
    ack[1] = aa;
    a = atecc_wake_own_addr(s_addr, 1u, &aa);
    if ((ack[0] != 0u || a != 0u) && hit == 0u) {
        hit = s_addr;
    }
    DebugUartLock();
    printf("[ATECC-H ] own-addr wake(0x%02X): x1 probe=%s(addr_byte=%s) "
           "x10 probe=%s(addr_byte=%s)\r\n",
           (unsigned)s_addr,
           ack[0] ? "ACK" : "NACK", ack[1] ? "ACK" : "NACK",
           a ? "ACK" : "NACK", aa ? "ACK" : "NACK");
    DebugUartUnlock();

    /* ---- Arm G：信标波形 + ACK 窗口实测 + 多采样复探 ----
     * 背景更正：示波器一度报"第 9 个时钟 SDA 被拉低"，但本臂与 Arm I 的实测一致表明
     * ——ACK 窗口内 SDA 一次都没低，而**同一帧内**我们自己拉的低读得回来
     * （ctrl_self_test=OK）→ 芯片确实不应答；此前观测应为 0xC0 的 6 个 0 位
     * （0xC0=1100_0000，SCL 翻转期间 SDA 持续为低，肉眼极像 ACK）或控制器件真实的 ACK。
     * 本臂同时给示波器一段干净可判定的波形：500µs 标记 + 32 个相同的 0xC0 字节。 */
    {
        uint8_t n_ack = 0u;
        uint8_t n_ms = 0u;
        uint8_t n_low = 0u;
        uint8_t ctrl_ok = 0u;
        uint32_t r2l = 0xFFFFFFFFu;
        uint32_t lows = 0u;
        uint32_t nsmp = 0u;
        uint8_t tail = 0u;
        BSP_I2C_UI_ProbeTiming tm;

        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        /* G1 信标：持锁 → 500µs 标记脉冲（= 一次合规唤醒，兼示波器触发特征）
         * → 等 tWHI → 背靠背探 0x60 x N（首帧紧跟一次真唤醒；不逐帧唤醒，波形才干净） */
        BSP_I2C_UI_Lock();
        BSP_I2C_UI_MarkerPulse(500u);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        for (i = 0u; i < DSO_ATECC_BEACON_N; i++) {
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                n_ack++;
            }
        }
        BSP_I2C_UI_Unlock();

        /* G2 计时探针：唤醒+等待+探测一体（同时实测 tWLO/tWHI），跑 3 次取末次 */
        for (i = 0u; i < 3u; i++) {
            BSP_I2C_UI_Lock();
            (void)BSP_I2C_UI_WakeProbeTimed(0x60u, 100u,
                                            DSO_ATECC_TWHI_MS * 1000u, &tm);
            BSP_I2C_UI_Unlock();
            r2l = tm.rise2low_ns;
            lows = tm.low_ns;
            nsmp = tm.n_samples;
            tail = tm.tail_low;
            if (r2l != 0xFFFFFFFFu) {
                n_low++;
            }
            if (tm.ctrl_low != 0u) {
                ctrl_ok = 1u;
            }
        }

        /* G3 多采样复探：判据改为"高电平期内任一次为低即 ACK"，同样逐帧唤醒 */
        BSP_I2C_UI_SetAckMultiSample(1u);
        BSP_I2C_UI_Lock();
        for (i = 0u; i < DSO_ATECC_BEACON_N; i++) {
            (void)BSP_I2C_UI_WakePulseMeasUs(100u);
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                n_ms++;
            }
        }
        BSP_I2C_UI_Unlock();
        BSP_I2C_UI_SetAckMultiSample(0u);
        BSP_I2C_UI_SetBitDelayUs(0u);

        if (n_ms != 0u && hit == 0u) {
            hit = 0x60u;
        }
        DebugUartLock();
        printf("[ATECC-G ] beacon 0x60 x%u: ACK=%u/%u (first frame after 500us wake)\r\n",
               (unsigned)DSO_ATECC_BEACON_N, (unsigned)n_ack,
               (unsigned)DSO_ATECC_BEACON_N);
        printf("[ATECC-G ] ack-timed 0x60 x3: low_seen=%u/3 samples=%lu rise2low=%luns "
               "low_width=%luns tail=%s ctrl_self_test=%s\r\n",
               (unsigned)n_low, (unsigned long)nsmp, (unsigned long)r2l,
               (unsigned long)lows, tail ? "LOW(=ACK)" : "HIGH(=NACK)",
               (ctrl_ok != 0u) ? "OK(read our own low)" : "FAIL");
        printf("[ATECC-G ] multisa 0x60 x%u (wake each): ACK=%u/%u -> %s\r\n",
               (unsigned)DSO_ATECC_BEACON_N, (unsigned)n_ms,
               (unsigned)DSO_ATECC_BEACON_N,
               (n_ms != 0u) ? "FIXED: ACK sampling was the bug"
                            : "still NACK (see ack-timed + scope)");
        DebugUartUnlock();
    }

    /* ---- Arm I：规范时序合规报告（tWLO ≥60µs，其后 tWHI ≥1500µs）----
     * 用户要求核查的两项都在**同一帧内 DWT 实测**：低电平实际宽度 + 唤醒释放到地址帧
     * 首个边沿的实际间隔。四组参数含"正好卡规格边界"的组合；判定与打印同源于头文件
     * 的 DSO_ATECC_TWLO_MIN_US / DSO_ATECC_TWHI_MIN_US。全程只读。 */
    {
        static const uint32_t lo_tab[4] = { 60u,  100u, 100u, 300u  };
        static const uint32_t hi_tab[4] = { 1500u, 1500u, 2000u, 5000u };
        uint8_t n_ack = 0u;
        uint8_t lo_pass = 0u;
        uint8_t hi_pass = 0u;
        uint8_t a;
        BSP_I2C_UI_ProbeTiming tm;

        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        DebugUartLock();
        printf("[ATECC-I ] spec: tWLO>=%uus then tWHI>=%uus (both measured in-frame by DWT)\r\n",
               (unsigned)DSO_ATECC_TWLO_MIN_US, (unsigned)DSO_ATECC_TWHI_MIN_US);
        DebugUartUnlock();
        for (i = 0u; i < 4u; i++) {
            BSP_I2C_UI_Lock();
            a = (uint8_t)((BSP_I2C_UI_WakeProbeTimed(0x60u, lo_tab[i], hi_tab[i],
                                                    &tm) == 0) ? 1u : 0u);
            BSP_I2C_UI_Unlock();
            if (a != 0u) {
                n_ack++;
                if (hit == 0u) {
                    hit = 0x60u;
                }
            }
            if (tm.t_lo_us >= DSO_ATECC_TWLO_MIN_US) { lo_pass++; }
            if (tm.t_hi_us >= DSO_ATECC_TWHI_MIN_US) { hi_pass++; }
            DebugUartLock();
            printf("[ATECC-I ] #%u lo=%uus hi=%uus -> tWLO=%luus(%s) tWHI=%luus(%s) "
                   "win_samples=%lu tail=%s ctrl=%s ack=%s%s\r\n",
                   (unsigned)(i + 1u), (unsigned)lo_tab[i], (unsigned)hi_tab[i],
                   (unsigned long)tm.t_lo_us,
                   (tm.t_lo_us >= DSO_ATECC_TWLO_MIN_US) ? "PASS" : "FAIL",
                   (unsigned long)tm.t_hi_us,
                   (tm.t_hi_us >= DSO_ATECC_TWHI_MIN_US) ? "PASS" : "FAIL",
                   (unsigned long)tm.n_samples,
                   tm.tail_low ? "LOW" : "HIGH",
                   tm.ctrl_low ? "OK" : "FAIL",
                   a ? "ACK" : "NACK",
                   (i == 2u) ? "  <- current impl 100/2000" : "");
            DebugUartUnlock();
        }
        BSP_I2C_UI_SetBitDelayUs(0u);
        DebugUartLock();
        printf("[ATECC-I ] timing verdict: tWLO %u/4 PASS  tWHI %u/4 PASS  ack=%u/4 -> %s\r\n",
               (unsigned)lo_pass, (unsigned)hi_pass, (unsigned)n_ack,
               ((lo_pass == 4u) && (hi_pass == 4u))
                   ? "spec timing SATISFIED (not the cause)"
                   : "spec timing VIOLATED -> fix the waits");
        DebugUartUnlock();
    }

    /* ---- Arm J：通用呼叫地址 0x00 专项探测（补上最后一块覆盖缺口）----
     * 事实：我们的全地址扫描从 **0x08 起**，0x00 从未被当作地址探过；而 Arm B 观测到
     * `gc_ack=1`（有器件应答通用呼叫）—— 这此前被**归因**于 INA226，但从未证实。
     * 若芯片的 I2C 地址被改写过（使用说明：地址"可改 1 次"），它可能就在 0x00。
     * 判据：把 s_addr 临时设为 0x00，跑一遍**完整只读** ATECC 命令（Info）——
     * 真 ATECC 会给出合法的 Count+数据+CRC16(0xA001) 响应帧，其它器件给不出。
     * 只读：无配置区写、无 Lock、无 OTP（0x03/0x07/0x30/0x00 等字节也不会触发
     * INA226 的通用呼叫专用动作——那些只认 0x04/0x06）。 */
    {
        uint8_t rev4[4] = { 0u, 0u, 0u, 0u };
        uint8_t old_addr = s_addr;
        int jrc;

        s_addr = 0x00u;
        jrc = DSO_Atecc608b_InfoRevision(rev4);
        DebugUartLock();
        printf("[ATECC-J ] addr 0x00 (general call, last coverage gap): Info rc=%d "
               "st=0x%02X rev=%02X%02X%02X%02X -> %s\r\n",
               jrc, (unsigned)DSO_Atecc608b_LastStatus(),
               (unsigned)rev4[0], (unsigned)rev4[1],
               (unsigned)rev4[2], (unsigned)rev4[3],
               (jrc == 0) ? "ATECC-LIKE RESPONSE AT 0x00" : "no ATECC response");
        DebugUartUnlock();
        if (jrc != 0) {
            s_addr = old_addr;      /* 未命中：还原工作地址 */
        } else {
            j00 = 1u;               /* 命中：地址 0x00 上有 ATECC 型响应 */
        }
    }

    /* ---- Arm K：0x60 / 0x35 走**完整命令流**（与 Arm J 对 0x00 同款方法）----
     * 此前 0x60/0x35/0x36 只用"地址字节是否 ACK"判定；本臂把**完整的 Info 命令组**
     * （word 0x03 + Count/Op/P1/P2 + CRC16，CRC 现场计算）发给它们：
     *   K1 逐字节记录 ACK 掩码（DiagTxBytes 即使在地址 NACK 后也继续发完，故能看出
     *      "死在哪一位"）；K2 再走官方驱动路径 DSO_Atecc608b_Exec（含内部唤醒、
     *      命令组、读轮询、响应 CRC 校验），成功则直接拿到 4 字节 RevNum。
     * 全程只读（Info 是纯读命令）——无 Write、无 Lock、无 OTP。 */
    {
        static const uint8_t kaddrs[2] = { 0x60u, 0x35u };
        uint8_t kframe[8];
        uint8_t kack[10];
        uint8_t krev[4];
        uint16_t kcrc;
        uint8_t ka;
        int krc;

        /* 与 atecc_exec_body 的 Info(Revision) 帧同构；CRC 现场算，避免魔数 */
        kframe[0] = DSO_ATECC608B_WORD_CMD;   /* 0x03 */
        kframe[1] = 7u;                       /* Count = 7 + 0 数据 */
        kframe[2] = ATECC_OP_INFO;            /* 0x30 */
        kframe[3] = 0x00u;                    /* P1 = Revision */
        kframe[4] = 0x00u;                    /* P2 lo */
        kframe[5] = 0x00u;                    /* P2 hi */
        kcrc = atecc_crc16(&kframe[1], 5u);   /* CRC 覆盖 Count..P2hi（5 字节） */
        kframe[6] = (uint8_t)(kcrc & 0xFFu);
        kframe[7] = (uint8_t)(kcrc >> 8);

        for (ka = 0u; ka < 2u; ka++) {
            /* K1 逐字节 ACK 掩码（按会话档 + 多点采样判 ACK） */
            BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);
            BSP_I2C_UI_Lock();
            (void)BSP_I2C_UI_WakePulseMeasUs(100u);
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            krc = BSP_I2C_UI_DiagTxBytes(kaddrs[ka], kframe, 8u, kack);
            BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
            BSP_I2C_UI_Unlock();
            DebugUartLock();
            printf("[ATECC-K ] 0x%02X write-frame(9B) rc=%d ack[addr=%u w03=%u cnt=%u op=%u "
                   "p1=%u p2l=%u p2h=%u crcl=%u crch=%u]\r\n",
                   (unsigned)kaddrs[ka], krc,
                   (unsigned)kack[0], (unsigned)kack[1], (unsigned)kack[2],
                   (unsigned)kack[3], (unsigned)kack[4], (unsigned)kack[5],
                   (unsigned)kack[6], (unsigned)kack[7], (unsigned)kack[8]);
            DebugUartUnlock();

            /* K2 完整命令流：先 GPIO 脉冲唤醒，再走官方驱动路径（内部再唤醒一次、
             * 组包、读轮询、响应 CRC 校验）——这就是"真正读芯片"的那条路 */
            krev[0] = 0u; krev[1] = 0u; krev[2] = 0u; krev[3] = 0u;
            BSP_I2C_UI_Lock();
            (void)BSP_I2C_UI_WakePulseMeasUs(100u);
            BSP_I2C_UI_Unlock();
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            s_addr = kaddrs[ka];
            krc = DSO_Atecc608b_InfoRevision(krev);
            DebugUartLock();
            printf("[ATECC-K ] 0x%02X full-flow Info: rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
                   (unsigned)kaddrs[ka], krc,
                   (unsigned)DSO_Atecc608b_LastStatus(),
                   (unsigned)krev[0], (unsigned)krev[1],
                   (unsigned)krev[2], (unsigned)krev[3],
                   (krc == 0) ? "<ATECC608B RESPONDED!>" : "(no response)");
            DebugUartUnlock();
            if (krc == 0) {
                hit = kaddrs[ka];
            }
        }
        s_addr = DSO_ATECC608B_I2C_ADDR;
    }

    /* ---- Arm L：逐条落实用户四方案（L1 已在任务最早时机执行）----
     * 方案 2/3c 已实现（GPIO 脉冲唤醒、General Call 0x00，见 Arm A/B）；
     * 这里补方案 3a/3b/4 的三个**未做过的变体**：
     *   L2 = 0x00 法降到 25kHz（20µs 半周期，满足"≤50kHz 做 Wake"）
     *   L3 = 0x00 法**不发 STOP**（正确实现：停在 SCL 低→先释放 SDA→再抬 SCL）
     *   L4 = 一次唤醒 + 2ms 间隔重试 5 次（GPIO 法 / 0x00 法各一组）
     * 全程只读。 */
    {
        uint8_t l2_ack;
        uint8_t l3_ack;
        uint8_t l4_gpio = 0u;
        uint8_t l4_zb = 0u;
        uint8_t lr;

        /* L2 */
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakeZeroBytesAt(3u, 20u, 1u);      /* 25kHz + 标准 STOP */
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        l2_ack = (uint8_t)((BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u);
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();

        /* L3 */
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakeZeroBytesAt(3u, 20u, 0u);      /* 25kHz + 不发 STOP */
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        l3_ack = (uint8_t)((BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u);
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();

        /* L4a：GPIO 唤醒一次 → 2ms 间隔重试 5 次 */
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakePulseMeasUs(100u);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        for (lr = 0u; lr < 5u; lr++) {
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                l4_gpio = (uint8_t)(lr + 1u);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2u));
        }
        BSP_I2C_UI_Unlock();

        /* L4b：0x00 法唤醒一次 → 2ms 间隔重试 5 次 */
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakeZeroBytesAt(3u, I2C_UI_SLOW_HALF_DELAY_US, 1u);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
        for (lr = 0u; lr < 5u; lr++) {
            if (BSP_I2C_UI_Probe(0x60u) == 0) {
                l4_zb = (uint8_t)(lr + 1u);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(2u));
        }
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();

        if ((l2_ack | l3_ack | l4_gpio | l4_zb) != 0u && hit == 0u) {
            hit = 0x60u;
        }
        DebugUartLock();
        printf("[ATECC-L2] 0x00 x3 @25kHz stop=1  -> 0x60=%s\r\n",
               l2_ack ? "ACK" : "NACK");
        printf("[ATECC-L3] 0x00 x3 @25kHz stop=0 (no STOP) -> 0x60=%s\r\n",
               l3_ack ? "ACK" : "NACK");
        printf("[ATECC-L4] one wake then retry5 @2ms: gpio=%s zb0x00=%s\r\n",
               l4_gpio ? "ACK" : "NACK(5/5)", l4_zb ? "ACK" : "NACK(5/5)");
        DebugUartUnlock();
    }

    /* ---- Arm M：**逻辑分析仪判定夹具**（专为拍摄优化，裁定"硬件 vs 软件"）----
     * 思路：让同一屏里同时出现"嫌疑器件"与"必然应答的对照器件"，用独立仪表的
     * I2C 协议解码器读数：
     *   解码 0x60=NACK 而 0x44/0x41=ACK → 位时序/速率/判据/总线/锁全部无罪 → 器件问题；
     *   解码 0x60=ACK 而固件报 NACK   → 我们采样读错 → 软件问题。
     * 波形：1ms 的 SDA 低电平标记（唯一触发特征，比 Arm G 的 500µs 标记更长）
     *   → 等 tWHI → 依次探 0x60 / 0x35 / 0x44 / 0x41 → 重复 3 轮；
     *   全程持锁、探针跑在 25kHz 解码友好档（标记本身不涉时钟，与档位无关）。只读。 */
    {
        static const uint8_t maddrs[4] = { 0x60u, 0x35u, 0x44u, 0x41u };
        uint8_t mack[4];
        uint8_t msum[4] = { 0u, 0u, 0u, 0u };
        uint8_t mround;
        uint8_t mi;

        for (mround = 0u; mround < 3u; mround++) {
            BSP_I2C_UI_Lock();
            BSP_I2C_UI_MarkerPulse(1000u);              /* 1ms：唯一触发特征 */
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
            BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_LA_SLOW_US);   /* 25kHz，便于解码 */
            for (mi = 0u; mi < 4u; mi++) {
                mack[mi] = (uint8_t)((BSP_I2C_UI_Probe(maddrs[mi]) == 0) ? 1u : 0u);
                if (mack[mi] != 0u) {
                    msum[mi]++;
                }
            }
            BSP_I2C_UI_SetBitDelayUs(0u);            /* 先还原档位再解锁 */
            BSP_I2C_UI_Unlock();
            DebugUartLock();
            printf("[ATECC-M%u] 0x60=%s 0x35=%s | ctrl 0x44=%s 0x41=%s (expect: NACK NACK ACK ACK)\r\n",
                   (unsigned)(mround + 1u),
                   mack[0] ? "ACK" : "NACK", mack[1] ? "ACK" : "NACK",
                   mack[2] ? "ACK" : "NACK", mack[3] ? "ACK" : "NACK");
            DebugUartUnlock();
        }

        if ((msum[0] | msum[1]) != 0u && hit == 0u) {
            hit = (msum[0] != 0u) ? 0x60u : 0x35u;
        }
        DebugUartLock();
        printf("[ATECC-M ] LA fixture: 3 rounds, 1ms marker each; decode the SAME capture: "
               "0x60/0x35=NACK + 0x44/0x41=ACK -> hardware; 0x60=ACK -> our sampling\r\n");
        DebugUartUnlock();
    }

    /* ---- Arm C：Wake→寻址 等待档（是否只是 tWHI 窗口不对） ---- */
    DebugUartLock();
    printf("[ATECC-C ] tWHI sweep(0x%02X):", (unsigned)s_addr);
    DebugUartUnlock();
    for (i = 0u; i < 3u; i++) {
        a = atecc_probe_after_wait(s_addr, wms[i]);
        if (a != 0u && hit == 0u) {
            hit = s_addr;
        }
        DebugUartLock();
        printf(" %ums=%s", (unsigned)wms[i], a ? "ACK" : "NACK");
        DebugUartUnlock();
    }
    DebugUartLock();
    printf("\r\n");
    DebugUartUnlock();

    /* ---- Arm D：位速率档（是否芯片 ACK 慢、主控采样过早误判 NACK） ---- */
    DebugUartLock();
    printf("[ATECC-D ] bitrate sweep:");
    DebugUartUnlock();
    for (i = 0u; i < 4u; i++) {
        a  = atecc_probe_at_rate(0x60u, bds[i]);
        if (a != 0u && hit == 0u) {
            hit = 0x60u;
        }
        ack[0] = a;
        a = atecc_probe_at_rate(0x35u, bds[i]);
        if (a != 0u && hit == 0u) {
            hit = 0x35u;
        }
        DebugUartLock();
        printf(" %uus:0x60=%s/0x35=%s", (unsigned)bds[i],
               ack[0] ? "ACK" : "NACK", a ? "ACK" : "NACK");
        DebugUartUnlock();
    }
    DebugUartLock();
    printf("\r\n");
    DebugUartUnlock();

    /* ---- Arm E：差分全地址扫描（醒态 @ 识别档 + 慢档，与 Arm 0 冷态基线相比）----
     * 差集里的地址 = 只可能出现在醒态的器件 = ATECC 候选（地址非候选值也能逮到）。
     * 只在**没有候选应答**时才跑：已确定在 0x60 就没必要再花 ~2.5s 扫全地址。
     * （本档位已降到 2kHz，扫描很慢；这是判定电池里最贵的一臂。） */
    if (hit == 0u) {
        /* 快档 + 2 地址/唤醒：整批 ~0.3ms，稳稳落在唤醒窗口内（原用 500µs 识别档，
         * 8 个地址一批要 ~80ms 才探完，若芯片只在唤醒后短时可访问整批都会被漏掉）。 */
        nw  = atecc_scan_collect(woke, (uint8_t)sizeof(woke), 1u,
                                 DSO_ATECC_SCAN_FAST_US, 2u);
        nw8 = atecc_scan_collect(woke8, (uint8_t)sizeof(woke8), 1u,
                                 DSO_ATECC_SCAN_SLOW_US, 4u);
        atecc_diag_addrs("[ATECC-E ] woke@fast(2/wake)", woke, nw);
        atecc_diag_addrs("[ATECC-E ] woke@8kHz(4/wake)", woke8, nw8);
        nd  = atecc_diag_diff("[ATECC-E ] diff@fast(woke-cold)", cold, nc, woke, nw);
        nd8 = atecc_diag_diff("[ATECC-E ] diff@8kHz(woke-cold)", cold, nc, woke8, nw8);
    } else {
        DebugUartLock();
        printf("[ATECC-E ] skipped: dev already answered at 0x%02X\r\n", (unsigned)hit);
        DebugUartUnlock();
    }

#if DSO_ATECC_DIAG_ARM_F
    /* ---- Arm F：唤醒脉冲宽度扫描 + 总线恢复后重试 ---- */
    DebugUartLock();
    printf("[ATECC-F ] pulse-width sweep(0x%02X):", (unsigned)s_addr);
    DebugUartUnlock();
    for (i = 0u; i < 5u; i++) {
        BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
        BSP_I2C_UI_Lock();
        (void)BSP_I2C_UI_WakePulseMeasUs(pws[i]);
        vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));   /* 本臂用更宽的等待 */
        a = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁 */
        BSP_I2C_UI_Unlock();
        if (a != 0u && hit == 0u) {
            hit = s_addr;
        }
        DebugUartLock();
        printf(" %uus=%s", (unsigned)pws[i], a ? "ACK" : "NACK");
        DebugUartUnlock();
    }
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    BSP_I2C_UI_Lock();                       /* 恢复+唤醒+探针同一持锁窗口 */
    (void)BSP_I2C_UI_BusRecover(&sda2, &scl2);
    (void)BSP_I2C_UI_WakePulseMeasUs(100u);
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
    a = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();
    if (a != 0u && hit == 0u) {
        hit = s_addr;
    }
    DebugUartLock();
    printf(" | recover+100us=%s (SDA=%u SCL=%u)\r\n", a ? "ACK" : "NACK",
           (unsigned)sda2, (unsigned)scl2);
    DebugUartUnlock();
#endif

    /* ---- 结论行（5 个互斥结论，指向下一步动作） ---- */
    DebugUartLock();
    if (j00 != 0u) {
        printf("[ATECC-VERDICT] ATECC-like response at general-call addr 0x00 -> "
               "set DSO_ATECC608B_I2C_ADDR to 0x00 and re-run\r\n");
    } else if (hit != 0u) {
        printf("[ATECC-VERDICT] dev ACK at 0x%02X -> wake OK; go read-only Info/Config\r\n",
               (unsigned)hit);
    } else if ((nd + nd8) != 0u) {
        printf("[ATECC-VERDICT] dev maybe at diff addr (%u, see Arm E last 2 lines) -> "
               "set DSO_ATECC608B_I2C_ADDR to it and re-run\r\n",
               (unsigned)(nd + nd8));
    } else if (ctrl_ok != 0u) {
        printf("[ATECC-VERDICT] ctrl=OK dev=NONE -> firmware side CLOSED "
               "(addr 0x00+0x08-0x7F, 4 rates, 3 wake forms, spec timing tWLO/tWHI PASS, "
               "same-frame ctrl self-test OK): chip does not answer -> replace part / "
               "contact supplier\r\n");
    } else {
        printf("[ATECC-VERDICT] ctrl=NONE -> bus-level fault (pull-up / stuck low / "
               "bit timing), fix the bus first\r\n");
    }
    DebugUartUnlock();

    return hit;
}

/* Arm L1 —— **用户方案 1 逐字复刻**，且刻意安排在上电后**最早**时机执行
 * （POR 等待之后、1.2s 静默等待之前）：
 *   tPU 1ms → GPIO 唤醒 80µs（>tWLO 60µs）→ 等 tWHI 2ms → **立即**探 0x60；
 *   NACK 则间隔 5ms 重试，共 3 次（方案 4 的"唤醒后重试"也一并落在窗口内）。
 * 全程只读（仅地址探测）。@return 1=0x60 应答 */
static uint8_t atecc_diag_l1_early(void)
{
    uint8_t ack = 0u;
    uint8_t r;
    uint32_t meas;

    vTaskDelay(pdMS_TO_TICKS(1u));                        /* tPU 裕量（方案 1 要求 ≥1ms） */
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_SESSION_BIT_US);   /* 16kHz，低于方案 3 的 50kHz 上限 */
    BSP_I2C_UI_Lock();
    meas = BSP_I2C_UI_WakePulseMeasUs(80u);               /* GPIO 直接控 SDA 拉低 80µs */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));         /* tWHI 2ms（≥1.5ms） */
    for (r = 0u; r < 3u; r++) {
        if (BSP_I2C_UI_Probe(0x60u) == 0) {
            ack = 1u;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5u));                    /* 第一次可能 NACK → 5ms 后重试 */
    }
    BSP_I2C_UI_SetBitDelayUs(0u);   /* 先还原档位再解锁：同总线其它任务不跑慢档 */
    BSP_I2C_UI_Unlock();

    DebugUartLock();
    printf("[ATECC-L1] tPU=1ms wake=%uus(>60) tWHI=%ums retry3@5ms -> 0x60=%s (round %u/3)\r\n",
           (unsigned)meas, (unsigned)DSO_ATECC_TWHI_MS,
           ack ? "ACK" : "NACK", ack ? (unsigned)(r + 1u) : 3u);
    DebugUartUnlock();
    return ack;
}

static void atecc608b_probe_task(void *pv)
{
    uint8_t cfg[128];
    uint8_t rev[4];
    uint8_t hit;     /* 判定电池命中的 ATECC 地址（0=未探到） */
    int rc;
    (void)pv;

    /* 规范：上电 ≥20ms 后才可动 I2C（实测值由 Arm 0 打印）；识别档位延迟见
     * DSO_ATECC_BIT_DELAY_US（当前 500µs≈2kHz，给 ACK 多点采样留窗口） */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_POR_WAIT_MS));
    s_addr = DSO_ATECC608B_I2C_ADDR;

#if DSO_ATECC_DIAG_BATTERY
    /* 方案 1 逐字复刻（tPU→Wake→tWHI→立即探+重试）刻意放在**最早时机**执行，
     * 满足"上电后立即 Wake 并扫描"，然后再等启动打印出完跑其余各臂 */
    uint8_t l1 = atecc_diag_l1_early();
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_DIAG_SETTLE_MS));
    /* 分层判定实验（只读）：Arm 0 上电事实+冷态基线 / A GPIO脉冲窗口 / B 0x00字节法对照 /
     * H 自身地址字节唤醒 / G 信标+多采样 / I 时序合规 / J 0x00 / K 0x60·0x35 完整流 /
     * L 用户四方案 / C 等待档 / D 速率档 / E 冷态vs醒态差分扫描 / F 脉宽扫描，末行为结论 */
    hit = atecc608b_diag_battery();
    if (hit == 0u && l1 != 0u) {
        hit = 0x60u;
    }
#else
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);
    hit = (uint8_t)((DSO_Atecc608b_Wake(NULL, NULL) == 0) ? s_addr : 0u);
#endif

    if (hit != 0u) {
        s_addr = hit;
        rc = DSO_Atecc608b_InfoRevision(rev);
        DebugUartLock();
        printf("[ATECC] Info rc=%d st=0x%02X rev=%02X%02X%02X%02X %s\r\n",
               rc, (unsigned)DSO_Atecc608b_LastStatus(),
               (unsigned)rev[0], (unsigned)rev[1],
               (unsigned)rev[2], (unsigned)rev[3],
               (rc == 0 && rev[2] == 0x60u) ? "<ATECC608B>" : "(rev?)");
        DebugUartUnlock();
        if (rc == 0) {
            /* Config 区 4×32B 全区 dump（偏移语义按 ATECC608 存储器映射核对） */
            uint8_t blk;
            uint8_t i;
            char line[128];   /* "[ATECC] cfgXX:" + 32×" XX" + CRLF + NUL = 113 */
            int n;
            for (blk = 0u; blk < 4u; blk++) {
                if (DSO_Atecc608b_ReadConfig32(blk, &cfg[blk * 32u]) != 0) {
                    rc = -1;
                    break;
                }
            }
            if (rc != 0) {
                DebugUartLock();
                printf("[ATECC] config read failed rc=%d\r\n", rc);
                DebugUartUnlock();
            } else {
                for (blk = 0u; blk < 4u; blk++) {
                    n = sprintf(line, "[ATECC] cfg%02X:", (unsigned)(blk * 32u));
                    for (i = 0u; i < 32u; i++) {
                        n += sprintf(line + n, " %02X", cfg[blk * 32u + i]);
                    }
                    sprintf(line + n, "\r\n");
                    DebugUartLock();
                    printf("%s", line);
                    DebugUartUnlock();
                }
                DebugUartLock();
                printf("[ATECC] sn9=%02X%02X%02X%02X%02X%02X%02X%02X%02X "
                       "rev@08=%02X lockval@56=%02X lockcfg@57=%02X slotlock@58=%02X%02X\r\n",
                       cfg[0x00], cfg[0x01], cfg[0x02], cfg[0x03],
                       cfg[0x04], cfg[0x05], cfg[0x06], cfg[0x07],
                       cfg[ATECC_CFG_SN8_OFF], cfg[ATECC_CFG_REV_OFF],
                       cfg[ATECC_CFG_LOCK_VALUE_OFF], cfg[ATECC_CFG_LOCK_CONFIG_OFF],
                       cfg[ATECC_CFG_SLOT_LOCKED_OFF],
                       cfg[ATECC_CFG_SLOT_LOCKED_OFF + 1u]);
                printf("[ATECC] present=1 addr=0x%02X\r\n", (unsigned)s_addr);
                DebugUartUnlock();
            }
        }
    }

    BSP_I2C_UI_SetBitDelayUs(0u);   /* 恢复默认位延迟（同总线其它器件用） */
    vTaskDelete(NULL);
}

void DSO_Atecc608b_Init(void)
{
    /* 一次性探测任务（prio 3：防 MainTask GUI_Init 卡死 spin 饿死 prio-1） */
    if (xTaskCreate(atecc608b_probe_task, "AteccP", 512, NULL, 3, NULL)
            != pdPASS) {
        printf("[BOOT] FAIL AteccP\r\n");
    }
}
