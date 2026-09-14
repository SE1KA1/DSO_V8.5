/**
 * @file vram_sentinels.c
 * @brief 哨兵初始化、周期校验与失败计数。
 */

#include "vram_sentinels.h"
#include "bsp_fmc_sdram.h"

/* 编译期约束：与 SDRAM_APP_BUF(0xC0400000) 不重叠、逻辑帧在 Phys0 之前 */
typedef char vram_assert_sent0_is_phys0_end[(VRAM_SENTINEL0_ADDR == (VRAM_PHYSICAL_0_ADDR + VRAM_PHYS_FRAME_BYTES)) ? 1 : -1];
typedef char vram_assert_sent1_gap[(VRAM_SENTINEL1_ADDR == (VRAM_PHYSICAL_1_ADDR + VRAM_PHYS_FRAME_BYTES + VRAM_GAP_AFTER_PHYS1_BYTES)) ? 1 : -1];
typedef char vram_assert_logic_pool_before_phys0[((uint32_t)VRAM_LOGICAL_ADDR + 2U * (uint32_t)VRAM_LOGICAL_SUBFRAME_BYTES) <= (uint32_t)VRAM_PHYSICAL_0_ADDR ? 1 : -1];
typedef char vram_assert_sent1_before_app[(VRAM_SENTINEL1_ADDR + (VRAM_SENTINEL_WORDS * 4U)) <= SDRAM_APP_BUF ? 1 : -1];

/*
 * 勿在此文件 #include "core_cm7.h"：与 device 头/编译选项未一起时易触发
 * "Compiler generates FPU instructions for a device without an FPU"。
 * ARM Compiler 5 用行内 dsb 即可；其它工具链可为空（哨兵仍可用）。
 */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION < 6000000)
/*
*********************************************************************************************************
*   函 数 名: vram_sentinel_dsb
*   功能说明: vram_sentinel_dsb 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
static __inline void vram_sentinel_dsb(void)
{
    __asm { dsb sy }
}
#else
static __inline void vram_sentinel_dsb(void) { }
#endif

static uint32_t s_vram_sentinel_fail;

/*
*********************************************************************************************************
*   函 数 名: VRAM_Sentinels_Init
*   功能说明: VRAM_Sentinels_Init 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void VRAM_Sentinels_Init(void)
{
    uint32_t i;
    volatile uint32_t *p0 = (volatile uint32_t *)VRAM_SENTINEL0_ADDR;
    volatile uint32_t *p1 = (volatile uint32_t *)VRAM_SENTINEL1_ADDR;

    for (i = 0U; i < VRAM_SENTINEL_WORDS; i++) {
        p0[i] = VRAM_SENTINEL_PATTERN;
        p1[i] = VRAM_SENTINEL_PATTERN;
    }
    vram_sentinel_dsb();
    s_vram_sentinel_fail = 0U;
}

/*
*********************************************************************************************************
*   函 数 名: VRAM_Sentinels_Check
*   功能说明: VRAM_Sentinels_Check 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
int VRAM_Sentinels_Check(void)
{
    uint32_t   i;
    int        ok   = 1;
    const volatile uint32_t *p0 = (const volatile uint32_t *)VRAM_SENTINEL0_ADDR;
    const volatile uint32_t *p1 = (const volatile uint32_t *)VRAM_SENTINEL1_ADDR;

    for (i = 0U; i < VRAM_SENTINEL_WORDS; i++) {
        if (p0[i] != VRAM_SENTINEL_PATTERN || p1[i] != VRAM_SENTINEL_PATTERN) {
            ok = 0;
            break;
        }
    }
    if (ok == 0) {
        s_vram_sentinel_fail++;
    }
    return ok;
}

/*
*********************************************************************************************************
*   函 数 名: VRAM_Sentinels_FailCount
*   功能说明: VRAM_Sentinels_FailCount 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint32_t VRAM_Sentinels_FailCount(void)
{
    return s_vram_sentinel_fail;
}
