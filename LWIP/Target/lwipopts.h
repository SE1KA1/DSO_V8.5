/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : Target/lwipopts.h
  * Description        : This file overrides LwIP stack default configuration
  *                      done in opt.h file.
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#include "stm32h7xx_hal.h"

/* STM32CubeMX Specific Parameters */
#define WITH_RTOS 1
#define CHECKSUM_BY_HARDWARE 1

/* LwIP Stack Parameters */
#define ETH_RX_BUFFER_SIZE 1536
#define MEM_ALIGNMENT 4
#define LWIP_RAM_HEAP_POINTER 0x30004000
#define LWIP_SUPPORT_CUSTOM_PBUF 1
#define LWIP_ETHERNET 1
#define LWIP_DNS_SECURE 7
#define TCP_SND_QUEUELEN (4 * TCP_SND_BUF / TCP_MSS)
#define TCP_SNDLOWAT 1460
#define TCP_SNDQUEUELOWAT 8
#define TCP_WND_UPDATE_THRESHOLD 1460
#define LWIP_NETIF_LINK_CALLBACK 1
#define TCPIP_THREAD_STACKSIZE 2048
#define TCPIP_THREAD_PRIO osPriorityNormal
#define TCPIP_MBOX_SIZE 16
#define SLIPIF_THREAD_STACKSIZE 1024
#define SLIPIF_THREAD_PRIO 3
#define DEFAULT_THREAD_STACKSIZE 1024
#define DEFAULT_THREAD_PRIO 3
#define DEFAULT_UDP_RECVMBOX_SIZE 6
#define DEFAULT_TCP_RECVMBOX_SIZE 6
#define DEFAULT_ACCEPTMBOX_SIZE 6
#define RECV_BUFSIZE_DEFAULT 2000000000
#define LWIP_STATS 0
#define CHECKSUM_GEN_IP 0
#define CHECKSUM_GEN_UDP 0
#define CHECKSUM_GEN_TCP 0
#define CHECKSUM_GEN_ICMP6 0
#define CHECKSUM_CHECK_IP 0
#define CHECKSUM_CHECK_UDP 0
#define CHECKSUM_CHECK_TCP 0
#define CHECKSUM_CHECK_ICMP6 0

/* LwIP 大窗口稳定配置（MSS 1460 + 8×MSS 窗口，MEM 64K 放 SRAM1 0x30004000，余量充足） */
#define MEM_SIZE                65536
#define MEMP_NUM_TCP_SEG        32
#define MEMP_NUM_TCP_PCB        4
#define MEMP_NUM_UDP_PCB        2
#define MEMP_NUM_PBUF           32
#define MEMP_NUM_NETBUF         16
#define MEMP_NUM_FRAG_PBUF      6
#define MEMP_NUM_NETCONN        4
#define MEMP_NUM_TCPIP_MSG_API  8
#define MEMP_NUM_TCPIP_MSG_INPKT 16
#define PBUF_POOL_SIZE          32
#define MEMP_NUM_REASSDATA      2
#define MEMP_NUM_ARP_QUEUE      8
#define TCP_MSS                 1460
#define TCP_SND_BUF             (8 * TCP_MSS)   /* 11680 */
#define TCP_WND                 (8 * TCP_MSS)   /* 11680 */
#define LWIP_RAW                1
#define MEMP_NUM_RAW_PCB        2

#ifdef __cplusplus
}
#endif
#endif /*__LWIPOPTS__H__ */
