/**
 * @file ping.h
 * @brief DSO ICMP Ping module using LwIP raw API.
 */
#ifndef DSO_PING_H
#define DSO_PING_H

#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

void DSO_Ping_Init(void);
void DSO_Ping_Send(void);
void DSO_Ping_Task(void *pv);

#ifdef __cplusplus
}
#endif
#endif /* DSO_PING_H */
