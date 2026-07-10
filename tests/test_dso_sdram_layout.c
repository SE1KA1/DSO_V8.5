#include <stdint.h>
#include <stdio.h>

#include "bsp_sdram_layout.h"
#include "bsp_vram_layout.h"

static int require_eq_u32(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: expected 0x%08lX, got 0x%08lX\n",
                name,
                (unsigned long)expected,
                (unsigned long)actual);
        return 1;
    }
    return 0;
}

static int require_order(const char *name, uint32_t low_end, uint32_t high_start)
{
    if (low_end > high_start) {
        fprintf(stderr, "%s overlap: low_end=0x%08lX high_start=0x%08lX\n",
                name,
                (unsigned long)low_end,
                (unsigned long)high_start);
        return 1;
    }
    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= require_eq_u32("sdram end",
                         SDRAM_TOTAL_END,
                         (uint32_t)EXT_SDRAM_ADDR + (uint32_t)EXT_SDRAM_SIZE);
    rc |= require_eq_u32("gui heap start", SDRAM_GUI_HEAP_BUF, SDRAM_APP_BUF);
    rc |= require_eq_u32("record start", SDRAM_RECORD_BUF, SDRAM_GUI_HEAP_END);
    rc |= require_eq_u32("record end", SDRAM_RECORD_END, SDRAM_ROLL_HISTORY_BUF);
    rc |= require_eq_u32("roll end", SDRAM_ROLL_HISTORY_END, SDRAM_TOTAL_END);

    rc |= require_order("logical vram before app",
                        VRAM_LOGICAL_ADDR + VRAM_LOGICAL_SUBFRAME_BYTES,
                        SDRAM_APP_BUF);
    rc |= require_order("phys0 before phys1",
                        VRAM_PHYSICAL_0_ADDR + VRAM_PHYS_FRAME_BYTES,
                        VRAM_PHYSICAL_1_ADDR);
    rc |= require_order("phys1 before app",
                        VRAM_PHYSICAL_1_ADDR + VRAM_PHYS_FRAME_BYTES,
                        SDRAM_APP_BUF);
    rc |= require_order("gui heap before record", SDRAM_GUI_HEAP_END, SDRAM_RECORD_BUF);
    rc |= require_order("record before roll history", SDRAM_RECORD_END, SDRAM_ROLL_HISTORY_BUF);
    rc |= require_order("roll history within sdram", SDRAM_ROLL_HISTORY_END, SDRAM_TOTAL_END);

    if (SDRAM_RECORD_SIZE == 0u) {
        fprintf(stderr, "record size must be nonzero\n");
        rc = 1;
    }
    if (SDRAM_ROLL_HISTORY_SIZE != (3u * 1024u * 1024u)) {
        fprintf(stderr, "unexpected roll history size: %lu\n",
                (unsigned long)SDRAM_ROLL_HISTORY_SIZE);
        rc = 1;
    }

    if (rc == 0) {
        puts("all SDRAM layout tests passed");
    }
    return rc;
}
