/**
 * @file dso_atecc608b.c
 * @brief ATECC608B 驱动：I2C 命令/状态机（软件 I2C BSP_I2C_UI，PB7/PB8，0x60）。
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
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define ATECC_RETRY_MS   1u          /* 忙时轮询间隔 */
#define ATECC_PROBE_N    3u          /* 探测重试次数 */

/* Config 区锁状态偏移（真机联调核对：LockValue@0x15、LockConfig@0x44） */
#define ATECC_CFG_LOCK_VALUE_OFF  0x15u
#define ATECC_CFG_LOCK_CONFIG_OFF 0x44u

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
    /* 候选地址（Config 区可定制，非默认 0x60；含 0x50——仅当读探针 NACK 才发命令，
     * 内存映射型(EEPROM 等 R=ok)被自动跳过，不会误写）。 */
    static const uint8_t cand[] = { 0x60u, 0x7Cu, 0x50u, 0x64u, 0x68u, 0x6Au };
    uint8_t rev[4];
    uint8_t ci;
    uint8_t n;
    int rc = DSO_ATECC_ERR_TIMEOUT;
    for (ci = 0u; ci < (uint8_t)(sizeof(cand) / sizeof(cand[0])); ci++) {
        /* 唤醒后：写探针必须 ACK 且读探针必须 NACK（命令/状态型特征） */
        BSP_I2C_UI_WakePulse();
        vTaskDelay(pdMS_TO_TICKS(25u));   /* tWHI≥1.5ms；SelfTest 使能时 tWHIST=20ms */
        if (atecc_readprobe(cand[ci]) == 0) {
            continue;   /* 读也 ACK → 内存映射型（EEPROM 类），跳过，防误写 */
        }
        s_addr = cand[ci];
        for (n = 0u; n < ATECC_PROBE_N; n++) {
            rc = DSO_Atecc608b_InfoRevision(rev);
            if (rc == 0) {
                return 0;   /* s_addr 已是找到的地址 */
            }
            vTaskDelay(pdMS_TO_TICKS(20u));   /* 给足唤醒后/自检余量 */
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

/* 已知器件地址（不打标签跳过读探针） */
static int atecc_is_known(uint8_t addr)
{
    return (addr == 0x22u || addr == 0x40u || addr == 0x41u || addr == 0x44u);
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

static void atecc608b_probe_task(void *pv)
{
    uint8_t cfg0[32], cfg2[32];
    uint8_t rev[4];
    int rc;
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(50u));   /* 慢 POR 余量 */
    atecc608b_scan_addrs();

    /* 0x60 诊断：SDA 回读自检 + 唤醒脉冲宽度扫描（60/100/300/1000/5000µs） */
    {
        static const uint32_t wus[] = { 60u, 100u, 300u, 1000u, 5000u };
        uint8_t k;
        printf("[ATECC] wake-diag sda_low_probe=%d\r\n",
               BSP_I2C_UI_SdaLowProbe());
        /* 长拉低 1500ms：请用万用表直接量【芯片 SDA 封装脚】应≈0V（连通则被拉低） */
        printf("[ATECC] holding SDA low 1500ms - measure chip SDA pin now\r\n");
        BSP_I2C_UI_SdaLowMs(1500u);
        printf("[ATECC] released\r\n");
        /* SCL 翻转 2s：请用万用表量【芯片 SCL 封装脚】DC 电压——
         * 有翻转(≈1.5-2V)=SCL 到芯片；恒 3.3V=SCL 开路/认错脚 */
        printf("[ATECC] toggling SCL 2000ms - measure chip SCL pin DC now\r\n");
        {
            uint32_t t_end = (uint32_t)xTaskGetTickCount() + pdMS_TO_TICKS(2000u);
            while ((int32_t)(t_end - (uint32_t)xTaskGetTickCount()) > 0) {
                BSP_I2C_UI_Lock();
                (void)BSP_I2C_UI_Probe(0x60u);
                BSP_I2C_UI_Unlock();
            }
        }
        printf("[ATECC] scl toggle done\r\n");
        printf("[ATECC] wake-diag 0x60:");
        for (k = 0u; k < (uint8_t)(sizeof(wus) / sizeof(wus[0])); k++) {
            int wp;
            BSP_I2C_UI_WakePulseUs(wus[k]);
            vTaskDelay(pdMS_TO_TICKS(25u));
            BSP_I2C_UI_Lock();
            wp = BSP_I2C_UI_Probe(0x60u);
            BSP_I2C_UI_Unlock();
            printf(" w=%lu:wr=%d", (unsigned long)wus[k], wp);
            if (wp == 0) {
                s_addr = 0x60u;
                rc = DSO_Atecc608b_InfoRevision(rev);
                printf(" info=%d st=0x%02X", rc, (unsigned)DSO_Atecc608b_LastStatus());
            }
        }
        printf("\r\n");
    }
    /* 0x7C 逐字节诊断（0x7C=7bit 0x3E，总线唯一"命令型"未知器件）：
     *  word 判别：ATECC 只 ACK word 0x03，其他 word（如 0x00）该字节 NACK；
     *             普通寄存器型/EEPROM 器件对任意 word 都 ACK → 一测便知。
     *  Info 帧逐字节 ACK 图：定位 NACK 落在第几个字节（CRC 尾 NACK = 命令被拒/格式错）。 */
    {
        uint8_t b;
        int r;
        uint8_t cmd[7];
        uint8_t frame[8];
        uint8_t al[16];
        uint16_t c;
        uint8_t i;
        static const uint8_t w0[1] = { 0x00u };
        static const uint8_t w3[1] = { DSO_ATECC608B_WORD_CMD };

        printf("[ATECC7C] byte-diag start\r\n");
        BSP_I2C_UI_WakePulse();
        vTaskDelay(pdMS_TO_TICKS(25u));

        BSP_I2C_UI_Lock();
        r = BSP_I2C_UI_DiagTxBytes(0x7Cu, w0, 1u, al);
        BSP_I2C_UI_Unlock();
        printf("[ATECC7C] word=0x00: addr=%s data=%s\r\n",
               (r == 0) ? "ACK" : "NACK", al[1] ? "ACK" : "NACK");

        BSP_I2C_UI_Lock();
        r = BSP_I2C_UI_DiagTxBytes(0x7Cu, w3, 1u, al);
        BSP_I2C_UI_Unlock();
        printf("[ATECC7C] word=0x03: addr=%s data=%s\r\n",
               (r == 0) ? "ACK" : "NACK", al[1] ? "ACK" : "NACK");

        cmd[0] = 0x07u; cmd[1] = 0x30u; cmd[2] = 0x00u;
        cmd[3] = 0x00u; cmd[4] = 0x00u;
        c = atecc_crc16(cmd, 5u);
        cmd[5] = (uint8_t)(c & 0xFFu);
        cmd[6] = (uint8_t)(c >> 8);
        frame[0] = DSO_ATECC608B_WORD_CMD;
        for (i = 0u; i < 7u; i++) {
            frame[1u + i] = cmd[i];
        }
        BSP_I2C_UI_Lock();
        r = BSP_I2C_UI_DiagTxBytes(0x7Cu, frame, 8u, al);
        BSP_I2C_UI_Unlock();
        printf("[ATECC7C] info crc=0x%04X addr=%s ack=", (unsigned)c,
               (r == 0) ? "ACK" : "NACK");
        for (i = 0u; i < 8u; i++) {
            printf("%s", al[1u + i] ? "A" : "N");
        }
        printf("  <- [03 07 30 00 00 00 %02X %02X]\r\n",
               (unsigned)cmd[5], (unsigned)cmd[6]);

        r = -1;
        for (i = 0u; i < 30u; i++) {
            BSP_I2C_UI_Lock();
            r = BSP_I2C_UI_ReadRaw(0x7Cu, &b, 1u);
            BSP_I2C_UI_Unlock();
            if (r == 0) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1u));
        }
        if (r == 0) {
            printf("[ATECC7C] read ok first=0x%02X @%ums\r\n",
                   (unsigned)b, (unsigned)(i + 1u));
        } else {
            printf("[ATECC7C] read timeout (30ms)\r\n");
        }
    }

    /* 极慢速重放（位延迟 30µs ≈ 10kHz）：排除软件 I2C 采样过早把器件慢 ACK
     * 误判 NACK。若慢速下 word=0x03/Info 恢复 ACK 或有响应 → 位时序问题，
     * 修 bsp_i2c_ui 采样即可（也解释"换板依旧"）。诊断结束恢复默认 2µs。 */
    {
        uint8_t b;
        int r;
        uint8_t cmd[7];
        uint8_t frame[8];
        uint8_t al[16];
        uint16_t c;
        uint8_t i;
        uint8_t p60;

        BSP_I2C_UI_SetBitDelayUs(30u);
        BSP_I2C_UI_WakePulse();
        vTaskDelay(pdMS_TO_TICKS(25u));

        BSP_I2C_UI_Lock();
        p60 = (BSP_I2C_UI_Probe(0x60u) == 0) ? 1u : 0u;
        BSP_I2C_UI_Unlock();
        printf("[ATECC] slow 0x60 addr=%s\r\n", p60 ? "ACK" : "NACK");

        cmd[0] = 0x07u; cmd[1] = 0x30u; cmd[2] = 0x00u;
        cmd[3] = 0x00u; cmd[4] = 0x00u;
        c = atecc_crc16(cmd, 5u);
        cmd[5] = (uint8_t)(c & 0xFFu);
        cmd[6] = (uint8_t)(c >> 8);
        frame[0] = DSO_ATECC608B_WORD_CMD;
        for (i = 0u; i < 7u; i++) {
            frame[1u + i] = cmd[i];
        }
        BSP_I2C_UI_Lock();
        r = BSP_I2C_UI_DiagTxBytes(0x7Cu, frame, 8u, al);
        BSP_I2C_UI_Unlock();
        printf("[ATECC7C] slow-info addr=%s ack=", (r == 0) ? "ACK" : "NACK");
        for (i = 0u; i < 8u; i++) {
            printf("%s", al[1u + i] ? "A" : "N");
        }
        printf("  crc=0x%04X\r\n", (unsigned)c);

        r = -1;
        for (i = 0u; i < 30u; i++) {
            BSP_I2C_UI_Lock();
            r = BSP_I2C_UI_ReadRaw(0x7Cu, &b, 1u);
            BSP_I2C_UI_Unlock();
            if (r == 0) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1u));
        }
        BSP_I2C_UI_SetBitDelayUs(0u);   /* 恢复默认，勿影响同总线器件 */
        if (r == 0) {
            printf("[ATECC7C] slow read ok first=0x%02X\r\n", (unsigned)b);
        } else {
            printf("[ATECC7C] slow read timeout (30ms)\r\n");
        }
    }
    /* 唤醒是总线级的（与芯片地址无关）：醒后全地址重扫，芯片必在某一地址 ACK */
    printf("[ATECC] post-wake rescan:\r\n");
    atecc608b_scan_addrs();
    printf("[ATECC] probe candidates 0x60/0x7C/0x50/0x64/0x68/0x6A (R=nack only, eeprom-safe)\r\n");

    if (DSO_Atecc608b_Probe() != 0) {
        printf("[ATECC] present=0 (no chip / addr / wake)\r\n");
        vTaskDelete(NULL);
        return;
    }
    printf("[ATECC] present=1 at addr=0x%02X\r\n", (unsigned)s_addr);
    (void)DSO_Atecc608b_InfoRevision(rev);
    rc  = DSO_Atecc608b_ReadConfig32(0u, cfg0);   /* 0x15 锁状态 */
    rc |= DSO_Atecc608b_ReadConfig32(2u, cfg2);   /* 0x44 锁状态 */
    if (rc != 0) {
        printf("[ATECC] config read rc=%d\r\n", rc);
    } else {
        /* serial4 = 序列号前 4 字节；锁状态字节 hex（0x55/0xCC 语义联调核对） */
        printf("[ATECC] rev=%02X%02X%02X%02X\r\n",
               rev[0], rev[1], rev[2], rev[3]);
        printf("[ATECC] serial4=%02X%02X%02X%02X\r\n",
               cfg0[0], cfg0[1], cfg0[2], cfg0[3]);
        printf("[ATECC] lock_value@0x15=0x%02X lock_config@0x44=0x%02X\r\n",
               cfg0[ATECC_CFG_LOCK_VALUE_OFF],       /* 0x15 ∈ block0 */
               cfg2[ATECC_CFG_LOCK_CONFIG_OFF - 0x40u]);  /* 0x44 ∈ block2 */
    }
    printf("[ATECC] probe done\r\n");
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
