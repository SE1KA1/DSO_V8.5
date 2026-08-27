/**
 * @file fmc16_bus.c
 * @brief FMC16 物理层: FMC Bank1 16-bit 异步 NOR/SRAM 配置、GPIO、读写原语。
 *
 * 硬件映射（依据 STM32_FMC16_INTEGRATION_GUIDE.md §2.1）:
 *   STM32 侧:
 *   - FMC_NE1  = PD7  (AF12)  ← 需本文件配置
 *   - FMC_NOE  = PD4  (AF12)  ← 需本文件配置
 *   - FMC_NWE  = PD5  (AF12)  ← 需本文件配置
 *   - FMC_NWAIT= PD6  (不初始化, 保持非 FMC 复用)  ← 禁用
 *   - FMC_D0..D3   = PD14, PD15, PD0, PD1  (AF12)  ← 已由 SDRAM init 配置
 *   - FMC_D4..D12  = PE7..PE15             (AF12)  ← 已由 SDRAM init 配置
 *   - FMC_D13..D15 = PD8, PD9, PD10        (AF12)  ← 已由 SDRAM init 配置
 *
 *   XO2 侧（供对照）:
 *   - DB[0..4] 与 STM32 SPI4(PE2/PE4/PE5/PE6)/INT0(PE3) 及 PI 开关 X2_IO 侧重合
 *   - DB[11..12] 与 XO2 SDA/SCL 重合
 *   - FMC_NOE = X2_TDI, FMC_NWE = X2_TCK, FMC_NWAIT = X2_TMS (JTAG 复用)
 *
 * 关键约束:
 *   - BANK1 配置为 16-bit 宽度（HC574/OLED/AD7606 等 32-bit NE1 设备不参与此链路）。
 *   - XO2 不解码地址线，所有 NE1 读写均被 XO2 感知。
 *   - FMC16 通信期间通过 fmc16_bus_try_lock/unlock 互斥仲裁 NE1 访问。
 *   - PE2~PE6 必须从 SPI4/INT0 释放（输入高阻），不得推挽驱动。
 *   - XO2 JTAG 必须在 FMC 操作前断开（TDI/TCK/TMS 已复用）。
 */
#include "fmc16_bus.h"
#include "bsp.h"  /* For HAL_GPIO / HAL_SRAM APIs */

/* ---- 内部状态 ------------------------------------------------------------ */

static int s_bus_initialized = 0;
static int s_bus_locked = 0;

/* ---- 内部: GPIO 配置 ----------------------------------------------------- */

/**
 * @brief 配置 FMC16 所需的 GPIO 引脚。
 *
 * 仅需追加 FMC 控制线（数据线已由 SDRAM 初始化配置为 AF12）:
 *   - PD4 = FMC_NOE
 *   - PD5 = FMC_NWE
 *   - PD7 = FMC_NE1
 *
 * 同时释放 SPI4/INT0 引脚（PE2~PE6）为输入高阻，避免与 FMC_DB 侧的
 * X2_IO 网络推挽冲突。
 */
static void fmc16_bus_gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    /* ---- 时钟 ---- */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_FMC_CLK_ENABLE();

    /* 1. FMC 控制线: PD4(NOE), PD5(NWE), PD7(NE1) */
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF12_FMC;
    g.Pin       = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOD, &g);

    /* 2. 释放 SPI4/INT0 引脚 (PE2~PE6) 为输入高阻。
     *    这些引脚不是 FMC 数据线，但 XO2 侧 DB[0..4] 与之物理重合，
     *    必须保证不被推挽驱动。 */
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOE, &g);
}

/* ---- 公共 API: 初始化 ----------------------------------------------------- */

void fmc16_bus_init(void)
{
    if (s_bus_initialized) return;

    /* 1. GPIO 配置（FMC 控制线 + 释放 SPI4 引脚） */
    fmc16_bus_gpio_init();

    /* 2. FMC BANK1 时序配置 — 16-bit 异步 SRAM
     *    时序合同（依据 INTEGRATION_GUIDE §3.3, HCLK=200MHz → 1 cycle = 5ns）:
     *      AddressSetupTime      = 8   → 40ns  (NE1 建立到 NWE/NOE 有效)
     *      AddressHoldTime       = 12  → 60ns  (写/读周期之间高电平)
     *      DataSetupTime         = 12  → 60ns  (数据建立到 NWE↑, ≥40ns 硬边界)
     *      BusTurnAroundDuration = 20  → 100ns (读写换向空档)
     *    注: 合同中的 "DataHoldTime=20ns" 无对应 HAL 字段 (H7 异步 SRAM 无该寄存器)，
     *        其效果由 DataSetupTime + AddressHoldTime 共同覆盖。
     */
    {
        SRAM_HandleTypeDef hsram = {0};
        FMC_NORSRAM_TimingTypeDef timing = {0};

        hsram.Instance  = FMC_NORSRAM_DEVICE;
        hsram.Extended  = FMC_NORSRAM_EXTENDED_DEVICE;

        timing.AddressSetupTime      = 8;   /* 40ns */
        timing.AddressHoldTime       = 12;  /* 60ns */
        timing.DataSetupTime         = 12;  /* 60ns */
        timing.BusTurnAroundDuration = 20;  /* 100ns */
        timing.CLKDivision           = 0;   /* 异步模式, don't care */
        timing.DataLatency           = 0;   /* 异步模式, don't care */
        timing.AccessMode            = FMC_ACCESS_MODE_A;

        hsram.Init.NSBank             = FMC_NORSRAM_BANK1;
        hsram.Init.DataAddressMux     = FMC_DATA_ADDRESS_MUX_DISABLE;
        hsram.Init.MemoryType         = FMC_MEMORY_TYPE_SRAM;
        hsram.Init.MemoryDataWidth    = FMC_NORSRAM_MEM_BUS_WIDTH_16;  /* ← 16-bit */
        hsram.Init.BurstAccessMode    = FMC_BURST_ACCESS_MODE_DISABLE;
        hsram.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
        hsram.Init.WaitSignalActive   = FMC_WAIT_TIMING_BEFORE_WS;
        hsram.Init.WriteOperation     = FMC_WRITE_OPERATION_ENABLE;
        hsram.Init.WaitSignal         = FMC_WAIT_SIGNAL_DISABLE;       /* 无 NWAIT (关键) */
        hsram.Init.ExtendedMode       = FMC_EXTENDED_MODE_DISABLE;
        hsram.Init.AsynchronousWait   = FMC_ASYNCHRONOUS_WAIT_DISABLE;
        hsram.Init.WriteBurst         = FMC_WRITE_BURST_DISABLE;
        hsram.Init.ContinuousClock    = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
        hsram.Init.WriteFifo          = FMC_WRITE_FIFO_ENABLE;

        if (HAL_SRAM_Init(&hsram, &timing, &timing) != HAL_OK)
        {
            FMC16_ERR("HAL_SRAM_Init failed");
            return;
        }
    }

    s_bus_initialized = 1;
    FMC16_LOG("bus init OK (BANK1 16-bit NE1 @ 0x%08lX, 40/60/60/100ns)",
              (unsigned long)FMC16_BASE_ADDR);
}

void fmc16_bus_deinit(void)
{
    if (!s_bus_initialized) return;

    /* 把 FMC 控制线 PD4/PD5/PD7 释放回输入（安全默认态） */
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &g);

    s_bus_initialized = 0;
    FMC16_LOG("bus deinit");
}

/* ---- 公共 API: 读写 ------------------------------------------------------- */

/**
 * @brief 写单个半字。
 *
 * FMC16_REG 是 volatile uint16_t* 映射到 0x60000000。
 * 编译器生成 STRH 指令, FMC 控制器产生 NE1=0, NWE=0 的总线周期。
 */
void fmc16_bus_write(uint16_t data)
{
    FMC16_REG = data;
}

/**
 * @brief 读单个半字。
 *
 * 编译器生成 LDRH 指令, FMC 控制器产生 NE1=0, NOE=0 的总线周期。
 * XO2 TX 空闲时返回 0xFFFF。
 */
uint16_t fmc16_bus_read(void)
{
    return FMC16_REG;
}

void fmc16_bus_write_buffer(const uint16_t *buf, uint16_t len)
{
    /* 临界区内连续写。
     * 不使用 DMA 以避免 FMC FIFO 重排 —— 先直接循环写。
     * 后续可优化为使用 DMA2 的 M2M 模式配合 FMC 流控。
     */
    for (uint16_t i = 0; i < len; i++)
    {
        FMC16_REG = buf[i];
    }
}

void fmc16_bus_read_buffer(uint16_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        buf[i] = FMC16_REG;
    }
}

uint16_t fmc16_bus_peek(void)
{
    return FMC16_REG;  /* 读但不处理 —— XO2 侧 peek 语义由协议保证 */
}

/* ---- 公共 API: 总线仲裁 --------------------------------------------------- */

int fmc16_bus_try_lock(uint32_t timeout_ms)
{
    /* 简化实现：自旋锁。后续可替换为 FreeRTOS mutex。
     * FMC16 通信窗口极短 (<1ms 大多数场景)，自旋优于上下文切换。
     */
    uint32_t start = HAL_GetTick();
    while (s_bus_locked)
    {
        if (timeout_ms > 0 && (HAL_GetTick() - start) >= timeout_ms)
            return 0;
        /* 短延迟防 bus contention */
        for (volatile int d = 0; d < 10; d++) {}
    }
    s_bus_locked = 1;
    return 1;
}

void fmc16_bus_unlock(void)
{
    s_bus_locked = 0;
}

int fmc16_bus_is_locked(void)
{
    return s_bus_locked;
}

/* ---- 公共 API: 设备探测 --------------------------------------------------- */

int fmc16_bus_probe(void)
{
    /* 简单探测：连续读 N 次，如果返回值中有非 0xFFFF 的值，
     * 说明 XO2 在驱动总线（空闲态 XO2 高阻，上拉电阻给 0xFFFF）。
     */
    if (!s_bus_initialized) return 0;

    fmc16_bus_try_lock(FMC16_BUS_LOCK_TIMEOUT_MS);
    int non_idle = 0;
    for (int i = 0; i < 32; i++)
    {
        if (fmc16_bus_read() != FMC16_IDLE_WORD)
        {
            non_idle++;
        }
    }
    fmc16_bus_unlock();

    return (non_idle > 0) ? 1 : 0;
}

int fmc16_bus_wait_data_ready(uint32_t timeout_ms)
{
    if (!s_bus_initialized) return 0;

    uint32_t start = HAL_GetTick();
    while (1)
    {
        uint16_t v = fmc16_bus_read();
        if (v != FMC16_IDLE_WORD)
        {
            return 1;  /* XO2 驱动了总线 — 有数据 */
        }
        if (timeout_ms > 0 && (HAL_GetTick() - start) >= timeout_ms)
        {
            return 0;  /* 超时 */
        }
    }
}
