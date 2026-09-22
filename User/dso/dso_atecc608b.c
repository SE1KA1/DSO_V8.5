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
 * 供电常开（无需 wake；若联调遇睡眠需 SDA 拉低 ≥80µs 唤醒——预留钩子注释）。
 * 每帧独立持锁（BSP_I2C_UI_Lock/Unlock），等待在锁外，避免阻塞同总线器件。
 */
#include "dso_atecc608b.h"
#include "bsp_i2c_ui.h"
#include "debug_uart_lock.h"
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

int DSO_Atecc608b_Exec(uint8_t opcode, uint8_t p1, uint16_t p2,
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

/* ---- 唤醒（TNGTLS 规范） ---- */

static int atecc_readprobe(uint8_t addr);   /* 前置声明：供 Probe / 诊断使用 */

int DSO_Atecc608b_Wake(uint8_t *zero_acked, uint16_t *ready_ms)
{
    uint32_t t0;
    uint32_t el;
    uint8_t acked;
    uint8_t round;
    int p = -1;

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        /* 调度器未启动：不能 vTaskDelay，只能 GPIO 脉冲唤醒 */
        BSP_I2C_UI_WakePulse();
        if (zero_acked != NULL) { *zero_acked = 0u; }
        if (ready_ms != NULL)   { *ready_ms = 0xFFFFu; }
        return DSO_ATECC_ERR_TIMEOUT;
    }

    /* ≤100kHz 发 N 个 0x00（SDA 连续低 ≥60µs）→ 释放上升沿唤醒；速率在 BSP 内强制 */
    BSP_I2C_UI_Lock();
    acked = (uint8_t)BSP_I2C_UI_WakeZeroBytes(DSO_ATECC_WAKE_ZB_N);
    BSP_I2C_UI_Unlock();
    if (zero_acked != NULL) { *zero_acked = acked; }

    /* 规范：Wake 后 ≥tWHI(1.5ms) 才能访问；自检使能时 tWHIST≈20ms。
     * 先等 tWHI 探一次地址，未中就按 tWHIST 保守再等一轮。 */
    t0 = (uint32_t)xTaskGetTickCount();
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    for (round = 0u; round < 2u; round++) {
        BSP_I2C_UI_Lock();
        p = BSP_I2C_UI_Probe(s_addr);
        BSP_I2C_UI_Unlock();
        if (p == 0) {
            break;
        }
        if (round == 0u) {
            vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
        }
    }
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

/* 扫描并打标签：写 ACK；R=ok(读也 ACK，像 EEPROM) / R=nack(读 NACK，像 ATECC/命令型) */
static void atecc608b_scan_addrs(void)
{
    uint8_t addr;
    printf("[ATECC] i2c scan 0x08-0x7F (W):");
    BSP_I2C_UI_Lock();
    for (addr = 0x08u; addr <= 0x7Fu; addr++) {
        if (BSP_I2C_UI_Probe(addr) == 0) {
            printf(" 0x%02X", (unsigned)addr);
            if (atecc_is_known(addr)) {
                printf("(known)");
            }
        }
    }
    BSP_I2C_UI_Unlock();
    printf("\r\n[ATECC] read-probe:");
    for (addr = 0x08u; addr <= 0x7Fu; addr++) {
        if (atecc_is_known(addr)) {
            continue;
        }
        if (atecc_readprobe(addr) == 0) {
            printf(" 0x%02X:R=ok", (unsigned)addr);    /* 内存映射型（EEPROM 类） */
        } else {
            printf(" 0x%02X:R=nack", (unsigned)addr);  /* 命令/状态型（ATECC 特征） */
        }
    }
    printf("\r\n");
}

/* 兜底阶梯（0x60 未响应时，每项回答一个具体问题，≤6 行）：
 *  A) 更长 POR + 0x00 字节法再唤醒一轮 → 探 0x60/0x35/0x36
 *  B) GPIO 脉冲 wake 1ms/5ms 对照 —— 区分"0x00 字节法实现问题"vs"器件/接线问题"
 *  C) 全地址 scan（排在 wake 之后，若芯片在总线上应可见 0x60）
 *  D) 结论行 */
static void atecc608b_fallback_diag(void)
{
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;

    vTaskDelay(pdMS_TO_TICKS(200u));
    BSP_I2C_UI_Lock();
    a1 = (uint8_t)BSP_I2C_UI_WakeZeroBytes(4u);
    BSP_I2C_UI_Unlock();
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
    BSP_I2C_UI_Lock();
    a2 = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
    a3 = (uint8_t)((BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u);
    BSP_I2C_UI_Unlock();
    DebugUartLock();
    printf("[ATECC] fb zb-wake x4 +200ms por: ack00=%u 0x%02X=%s 0x60=%s\r\n",
           (unsigned)a1, (unsigned)s_addr, a2 ? "ACK" : "NACK", a3 ? "ACK" : "NACK");
    DebugUartUnlock();

    BSP_I2C_UI_WakePulseUs(1000u);
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
    BSP_I2C_UI_Lock();
    a1 = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_Unlock();
    BSP_I2C_UI_WakePulseUs(5000u);
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_STD_MS));
    BSP_I2C_UI_Lock();
    a2 = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
    BSP_I2C_UI_Unlock();
    DebugUartLock();
    printf("[ATECC] fb gpio-pulse 1ms:0x%02X=%s 5ms=%s\r\n",
           (unsigned)s_addr, a1 ? "ACK" : "NACK", a2 ? "ACK" : "NACK");
    DebugUartUnlock();

    /* 全地址 scan 排在 wake 之后：唤醒若生效，芯片应在此可见 */
    atecc608b_scan_addrs();

    /* ATECC 特征扫描（不依赖地址假设）：ATECC 只接受 word 0x03，
     * 故特征 = 地址 ACK **且** word 0x03 该字节也 ACK。普通 EEPROM 虽也
     * 两个都 ACK，但其 R=ok（内存型）；结合本函数上方 read-probe 结果即可区分。
     * 只要片子在总线上且已被唤醒，无论其地址是多少都会被此组合逮到。 */
    /* 特征扫描前再唤醒一次（扫描耗时较长，确保芯片处于醒态） */
    BSP_I2C_UI_Lock();
    (void)BSP_I2C_UI_WakeZeroBytes(DSO_ATECC_WAKE_ZB_N);
    BSP_I2C_UI_Unlock();
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_TWHI_MS));
    {
        static const uint8_t w3[1] = { DSO_ATECC608B_WORD_CMD };
        uint8_t ack[8];
        uint8_t ad;
        int r2;
        DebugUartLock();
        printf("[ATECC] feat-scan (W-ACK & word0x03-ACK):");
        DebugUartUnlock();
        for (ad = 0x08u; ad <= 0x7Fu; ad++) {
            if (atecc_is_known(ad)) {
                continue;
            }
            BSP_I2C_UI_Lock();
            r2 = BSP_I2C_UI_DiagTxBytes(ad, w3, 1u, ack);
            BSP_I2C_UI_Unlock();
            if (r2 == 0 && ack[1] != 0u) {
                DebugUartLock();
                printf(" 0x%02X", (unsigned)ad);
                DebugUartUnlock();
            }
        }
        DebugUartLock();
        printf("\r\n");
        DebugUartUnlock();
    }

    DebugUartLock();
    printf("[ATECC] present=0 (no chip / addr / wake) sda_low_probe=%d\r\n",
           BSP_I2C_UI_SdaLowProbe());
    DebugUartUnlock();
}

static void atecc608b_probe_task(void *pv)
{
    uint8_t cfg[128];
    uint8_t rev[4];
    uint8_t wmain;  /* W 探针：主地址 DSO_ATECC608B_I2C_ADDR */
    uint8_t rmain;  /* R 探针：主地址 */
    uint8_t alt60;  /* W 探针对照：SSHDA 0x60 */
    uint8_t alt35;  /* W 探针对照：TNGTLS 0x35 */
    uint8_t alt36;  /* W 探针对照：TNGTLS 0x36 */
    uint8_t zi = 0u;
    uint16_t rms = 0xFFFFu;
    int rc;
    (void)pv;

    /* 规范：上电 ≥20ms 后才可动 I2C；识别阶段位延迟 5µs（≈100kHz 标准模式） */
    vTaskDelay(pdMS_TO_TICKS(DSO_ATECC_POR_WAIT_MS));
    BSP_I2C_UI_SetBitDelayUs(DSO_ATECC_BIT_DELAY_US);

    s_addr = DSO_ATECC608B_I2C_ADDR;
    rc = DSO_Atecc608b_Wake(&zi, &rms);
    DebugUartLock();
    printf("[ATECC] addr7=0x%02X W=0x%02X R=0x%02X por=%ums zb0x00x%u@100k "
           "ack00=%u wake=%d ready=%ums\r\n",
           (unsigned)s_addr, (unsigned)((s_addr << 1) | 0u),
           (unsigned)((s_addr << 1) | 1u),
           (unsigned)DSO_ATECC_POR_WAIT_MS, (unsigned)DSO_ATECC_WAKE_ZB_N,
           (unsigned)zi, rc, (unsigned)rms);
    DebugUartUnlock();

    BSP_I2C_UI_Lock();
    wmain = (uint8_t)((BSP_I2C_UI_Probe(s_addr) == 0) ? 1u : 0u);
    alt60 = (uint8_t)((BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u);
    alt35 = (uint8_t)((BSP_I2C_UI_Probe(0x35u) == 0) ? 1u : 0u);
    alt36 = (uint8_t)((BSP_I2C_UI_Probe(0x36u) == 0) ? 1u : 0u);
    BSP_I2C_UI_Unlock();
    rmain = (uint8_t)((atecc_readprobe(s_addr) == 0) ? 1u : 0u);   /* 自带锁 */
    DebugUartLock();
    printf("[ATECC] probe W:0x%02X=%s R:0x%02X=%s | W:0x60=%s 0x35=%s 0x36=%s\r\n",
           (unsigned)s_addr, wmain ? "ACK" : "NACK",
           (unsigned)s_addr, rmain ? "ACK" : "NACK",
           alt60 ? "ACK" : "NACK", alt35 ? "ACK" : "NACK", alt36 ? "ACK" : "NACK");
    DebugUartUnlock();

    if (wmain != 0u && rmain == 0u) {
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
    } else {
        atecc608b_fallback_diag();
    }
    /* 旧 0x7C 逐字节诊断已移除：其前提（"ATECC 只 ACK word 0x03"）不成立
     * （ATECC 对写字节一律 ACK），结论不可靠，且已由上面的 0x35 规范流程取代。 */

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
