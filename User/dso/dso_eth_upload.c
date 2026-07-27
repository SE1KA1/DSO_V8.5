#include "dso_eth_upload.h"
#include "lwip.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/errno.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

#define ETH_UPLOAD_PORT 5005

static int s_cli=-1, s_srv=-1;
static SemaphoreHandle_t s_mtx;

void DSO_ETH_Upload_Init(void){ s_mtx=xSemaphoreCreateMutex(); }

void DSO_ETH_Upload_SendLine(const char *t)
{
    if(s_cli<0||!t) return;
    if(xSemaphoreTake(s_mtx,pdMS_TO_TICKS(10))!=pdTRUE) return;
    size_t n=strlen(t); lwip_send(s_cli,t,n,0);
    if(n<2||t[n-2]!='\r'||t[n-1]!='\n') lwip_send(s_cli,"\r\n",2,0);
    xSemaphoreGive(s_mtx);
}

void DSO_ETH_Upload_Task(void *pv)
{
    (void)pv;
    /* Flash backlight to confirm task entry */
    *(volatile uint32_t*)0x40020418 = (1U<<14); /* PB14 ON */
    for(volatile uint32_t d=0; d<5000000U; d++){}
    *(volatile uint32_t*)0x40020428 = (1U<<14); /* PB14 OFF */
    struct sockaddr_in a,c; socklen_t cl; int r;

    printf("[ETH] LwIP init...\r\n");
    /* PHY needs time to complete auto-negotiation:
     * reference project has 3s osDelay before MX_LWIP_Init. */
    vTaskDelay(pdMS_TO_TICKS(3000));
    MX_LWIP_Init();
    {
        struct netif *nf = netif_default;
        printf("[ETH] netif_def=%p up=%d ip=0x%lX\r\n",
               (void*)nf, (int)(nf?netif_is_up(nf):0),
               (unsigned long)(nf?ip4_addr_get_u32(netif_ip4_addr(nf)):0));
    }
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* UDP test: send to PC */
    {
        struct sockaddr_in dst;
        memset(&dst,0,sizeof(dst));
        dst.sin_family=AF_INET;
        dst.sin_addr.s_addr = PP_HTONL(0xC0A86464U); /* 192.168.100.100 */
        dst.sin_port = lwip_htons(5005);
        s_srv=lwip_socket(AF_INET,SOCK_DGRAM,0);
        printf("[ETH] UDP socket=%d\r\n",s_srv);
        const char *msg="DSO_UDP_TEST";
        for(int i=0;i<10;i++){
            int n=lwip_sendto(s_srv,msg,strlen(msg),0,(struct sockaddr*)&dst,sizeof(dst));
            printf("[ETH] UDP send#%d ret=%d errno=%d\r\n",i,n,errno);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        lwip_close(s_srv);
    }
    /* TCP server on port 5005 */
    s_srv = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s_srv < 0) { printf("[ETH] TCP socket fail errno=%d\r\n", errno); goto idle; }
    memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY;
    a.sin_port=lwip_htons(ETH_UPLOAD_PORT);
    if(lwip_bind(s_srv,(struct sockaddr*)&a,sizeof(a))<0){printf("[ETH] bind fail errno=%d\r\n",errno);lwip_close(s_srv);s_srv=-1;goto idle;}
    printf("[ETH] bind :%d OK\r\n",ETH_UPLOAD_PORT);
    if(lwip_listen(s_srv,1)<0){printf("[ETH] listen fail errno=%d\r\n",errno);lwip_close(s_srv);s_srv=-1;goto idle;}
    printf("[ETH] listening :%d\r\n",ETH_UPLOAD_PORT);

    for(;;){
        cl=sizeof(c);
        r=lwip_accept(s_srv,(struct sockaddr*)&c,&cl);
        if(r<0){static int acnt=0;if(++acnt%25==0)printf("[ETH] acc errno=%d #%d\r\n",errno,acnt);vTaskDelay(pdMS_TO_TICKS(200));continue;}
        s_cli=r;
        { char d; int t=0;
          while(lwip_recv(s_cli,&d,1,MSG_PEEK)>=0){
              vTaskDelay(pdMS_TO_TICKS(1000));
              if(++t>=2){t=0;DSO_ETH_Upload_SendLine("DSO_V8.5 ETH OK");}
          }
        }
        lwip_close(s_cli); s_cli=-1; vTaskDelay(pdMS_TO_TICKS(100));
    }
idle: for(;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
