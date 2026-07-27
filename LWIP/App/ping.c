/**
 * @file ping.c
 * @brief DSO ICMP Ping — actively pings 192.168.100.100 using LwIP raw API.
 */
#include "ping.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ip.h"
#include "lwip/prot/icmp.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

#define DSO_PING_TARGET_IP    PP_HTONL(0xC0A86464U)  /* 192.168.100.100 */
#define DSO_PING_INTERVAL_MS  2000U
#define DSO_PING_DATA_SIZE    32U
#define DSO_PING_ID           0x4453U                /* "DS" */

static struct raw_pcb *s_ping_pcb = NULL;
static uint16_t         s_ping_seq = 0;

/* ICMP Echo packet = LwIP's icmp_echo_hdr (8 bytes) + payload */
#pragma pack(1)
struct ping_pkt {
  struct icmp_echo_hdr hdr;
  uint8_t              data[DSO_PING_DATA_SIZE];
};
#pragma pack()

static void build_ping(struct ping_pkt *pkt, uint16_t seq)
{
  pkt->hdr.type   = ICMP_ECHO;
  pkt->hdr.code   = 0;
  pkt->hdr.chksum = 0;
  pkt->hdr.id     = PP_HTONS(DSO_PING_ID);
  pkt->hdr.seqno  = PP_HTONS(seq);
  memset(pkt->data, 0xAB, DSO_PING_DATA_SIZE);
  pkt->hdr.chksum = inet_chksum(pkt, sizeof(*pkt));
}

/* ---- raw API receive callback -------------------------------------------- */
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p,
                      const ip_addr_t *addr)
{
  struct ping_pkt *pkt;
  (void)arg;
  (void)pcb;

  if (p == NULL) return 0;

  pkt = (struct ping_pkt *)p->payload;
  if (p->len >= sizeof(struct ping_pkt) && pkt->hdr.type == ICMP_ER) {
    uint16_t rcv_id  = PP_NTOHS(pkt->hdr.id);
    uint16_t rcv_seq = PP_NTOHS(pkt->hdr.seqno);
    printf("[PING] reply from %s id=0x%04X seq=%u\r\n",
           ipaddr_ntoa(addr), (unsigned)rcv_id, (unsigned)rcv_seq);
    pbuf_free(p);
    return 1; /* eaten */
  }

  pbuf_free(p);
  return 0; /* not eaten, let LwIP handle */
}

/* ---- public API ---------------------------------------------------------- */
void DSO_Ping_Init(void)
{
  s_ping_pcb = raw_new(IP_PROTO_ICMP);
  if (s_ping_pcb == NULL) {
    printf("[PING] raw_new failed\r\n");
    return;
  }
  raw_recv(s_ping_pcb, ping_recv, NULL);
  raw_bind_netif(s_ping_pcb, netif_default);
  printf("[PING] init OK, target=192.168.100.100\r\n");
}

void DSO_Ping_Send(void)
{
  struct ping_pkt pkt;
  ip_addr_t target;
  struct pbuf *p;

  if (s_ping_pcb == NULL || netif_default == NULL) return;
  if (!netif_is_link_up(netif_default)) return;

  IP4_ADDR(&target,
           (uint8_t)((DSO_PING_TARGET_IP >> 24) & 0xFF),
           (uint8_t)((DSO_PING_TARGET_IP >> 16) & 0xFF),
           (uint8_t)((DSO_PING_TARGET_IP >>  8) & 0xFF),
           (uint8_t)((DSO_PING_TARGET_IP      ) & 0xFF));

  build_ping(&pkt, ++s_ping_seq);

  p = pbuf_alloc(PBUF_IP, sizeof(pkt), PBUF_RAM);
  if (p == NULL) return;
  memcpy(p->payload, &pkt, sizeof(pkt));
  raw_sendto(s_ping_pcb, p, &target);
  pbuf_free(p);
  printf("[PING] sent seq=%u to 192.168.100.100\r\n", (unsigned)s_ping_seq);
}

void DSO_Ping_Task(void *pv)
{
  (void)pv;

  for (;;) {
    if (netif_default != NULL && netif_is_link_up(netif_default)) break;
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  DSO_Ping_Init();

  for (;;) {
    DSO_Ping_Send();
    vTaskDelay(pdMS_TO_TICKS(DSO_PING_INTERVAL_MS));
  }
}
