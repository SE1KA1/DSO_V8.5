/**
 * @file bsp_sdram_layout.h
 * @brief SDRAM 应用区划分：GUI heap、Record buffer、RollHistory 等。
 */

#ifndef BSP_SDRAM_LAYOUT_H
/* 宏定义: BSP_SDRAM_LAYOUT_H - 头文件保护宏。 */
#define BSP_SDRAM_LAYOUT_H

#include <stdint.h>

#include "bsp_fmc_sdram.h"

/*
 * Checked ownership map for the 32 MB external SDRAM.
 *
 *   0xC0000000  Display frame buffers   (4 MB)
 *   0xC0400000  SDRAM_APP_BUF / GUI heap (up to 6 MB)
 *   0xC0A00000  SDRAM_RECORD_BUF
 *   0xC1D00000  SDRAM_ROLL_HISTORY_BUF  (final 3 MB)
 *   0xC2000000  SDRAM_TOTAL_END
 */
#define SDRAM_TOTAL_BEGIN        ((uint32_t)EXT_SDRAM_ADDR)
/* 宏定义: SDRAM_TOTAL_SIZE - SDRAM_TOTAL_SIZE 的宏定义。 */
#define SDRAM_TOTAL_SIZE         ((uint32_t)EXT_SDRAM_SIZE)
/* 宏定义: SDRAM_TOTAL_END - SDRAM_TOTAL_END 的宏定义。 */
#define SDRAM_TOTAL_END          (SDRAM_TOTAL_BEGIN + SDRAM_TOTAL_SIZE)

/* 宏定义: SDRAM_GUI_HEAP_BUF - SDRAM_GUI_HEAP_BUF 的宏定义。 */
#define SDRAM_GUI_HEAP_BUF       ((uint32_t)SDRAM_APP_BUF)
/* 宏定义: SDRAM_GUI_HEAP_RESERVE - SDRAM_GUI_HEAP_RESERVE 的宏定义。 */
#define SDRAM_GUI_HEAP_RESERVE   (6u * 1024u * 1024u)
/* 宏定义: SDRAM_GUI_HEAP_END - SDRAM_GUI_HEAP_END 的宏定义。 */
#define SDRAM_GUI_HEAP_END       (SDRAM_GUI_HEAP_BUF + SDRAM_GUI_HEAP_RESERVE)

/* 宏定义: SDRAM_ROLL_HISTORY_SIZE - SDRAM_ROLL_HISTORY_SIZE 的宏定义。 */
#define SDRAM_ROLL_HISTORY_SIZE  (3u * 1024u * 1024u)
/* 宏定义: SDRAM_ROLL_HISTORY_END - SDRAM_ROLL_HISTORY_END 的宏定义。 */
#define SDRAM_ROLL_HISTORY_END   SDRAM_TOTAL_END
/* 宏定义: SDRAM_ROLL_HISTORY_BUF - SDRAM_ROLL_HISTORY_BUF 的宏定义。 */
#define SDRAM_ROLL_HISTORY_BUF   (SDRAM_ROLL_HISTORY_END - SDRAM_ROLL_HISTORY_SIZE)

/* 宏定义: SDRAM_RECORD_BUF - SDRAM_RECORD_BUF 的宏定义。 */
#define SDRAM_RECORD_BUF         SDRAM_GUI_HEAP_END
/* 宏定义: SDRAM_RECORD_END - SDRAM_RECORD_END 的宏定义。 */
#define SDRAM_RECORD_END         SDRAM_ROLL_HISTORY_BUF
/* 宏定义: SDRAM_RECORD_SIZE - SDRAM_RECORD_SIZE 的宏定义。 */
#define SDRAM_RECORD_SIZE        (SDRAM_RECORD_END - SDRAM_RECORD_BUF)

/* 宏定义: BSP_SDRAM_STATIC_ASSERT - BSP_SDRAM_STATIC_ASSERT 的宏定义。 */
#define BSP_SDRAM_STATIC_ASSERT(name, expr) \
    typedef char bsp_sdram_static_assert_##name[(expr) ? 1 : -1]

BSP_SDRAM_STATIC_ASSERT(gui_heap_after_app_buf,
                        SDRAM_GUI_HEAP_BUF == (uint32_t)SDRAM_APP_BUF);
BSP_SDRAM_STATIC_ASSERT(record_after_gui_heap,
                        SDRAM_RECORD_BUF == SDRAM_GUI_HEAP_END);
BSP_SDRAM_STATIC_ASSERT(record_before_roll_history,
                        SDRAM_RECORD_END <= SDRAM_ROLL_HISTORY_BUF);
BSP_SDRAM_STATIC_ASSERT(roll_history_inside_sdram,
                        SDRAM_ROLL_HISTORY_END == SDRAM_TOTAL_END);
BSP_SDRAM_STATIC_ASSERT(record_region_nonzero,
                        SDRAM_RECORD_END > SDRAM_RECORD_BUF);

#endif /* BSP_SDRAM_LAYOUT_H */
