/**
 * @file cloud_bridge.h
 * @brief Cloud bridge FPGA initialization via PS configuration mode.
 *
 * Uses the same PS config pins as the main XC7A100 FPGA:
 *   PC6  = nCONFIG (PROGRAM_B)
 *   PC13 = CONF_DONE (input, monitor)
 *   PG12 = DCLK (configuration clock output)
 *   PI8  = DATA0 (configuration data output)
 *
 * Bitstream is read from QSPI Flash (W25Q256) at a fixed offset.
 */
#ifndef CLOUD_BRIDGE_H
#define CLOUD_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** QSPI Flash offset of the cloud bridge FPGA bitstream (RBF raw binary). */
#define CLOUD_BRIDGE_BITSTREAM_OFFSET  0x00400000U  /* 4MB offset, adjust per layout */
#define CLOUD_BRIDGE_BITSTREAM_MAX_SIZE (512U * 1024U)  /* 512KB max */

/** PS configuration timing (microseconds). */
#define CB_CFG_T_CFG_SETUP   2U    /* nCONFIG LOW→HIGH to CONF_DONE check */
#define CB_CFG_T_STATUS_MAX  50000U /* Max wait for nSTATUS HIGH (50ms) */
#define CB_CFG_T_DONE_MAX    500000U /* Max wait for CONF_DONE HIGH (500ms) */
#define CB_CFG_DCLK_HALF_US  2U    /* DCLK half-period in us */

/** Return codes. */
typedef enum {
    CB_OK = 0,
    CB_ERR_FLASH_READ  = -1,
    CB_ERR_TIMEOUT     = -2,
    CB_ERR_CONF_DONE   = -3,
    CB_ERR_SIZE        = -4,
} CloudBridgeStatus;

/**
 * @brief  Early-stage isolation: hold nCONFIG LOW, mute config clocks.
 *         Called from System_Init() BEFORE any FMC/SDRAM access.
 */
void CloudBridge_Isolate(void);

/**
 * @brief  Configure cloud bridge FPGA via PS mode.
 *         Called after SDRAM and LCD are initialized.
 *         Reads bitstream from QSPI Flash and sends it via DCLK/DATA0.
 * @return CB_OK on success, negative error code otherwise.
 */
CloudBridgeStatus CloudBridge_Configure(void);

/**
 * @brief  Check if FPGA configuration completed successfully.
 * @return 1 if CONF_DONE is HIGH, 0 otherwise.
 */
int CloudBridge_IsConfigured(void);

#ifdef __cplusplus
}
#endif
#endif /* CLOUD_BRIDGE_H */
