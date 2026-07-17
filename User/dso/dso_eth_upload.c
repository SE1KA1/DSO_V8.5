#include "dso_eth_upload.h"
#include "lwip.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

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
    struct sockaddr_in a,c; socklen_t cl; int r;

    MX_LWIP_Init();
    vTaskDelay(pdMS_TO_TICKS(5000));

    s_srv=lwip_socket(AF_INET,SOCK_STREAM,0);
    if(s_srv<0) goto idle;
    memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY;
    a.sin_port=lwip_htons(ETH_UPLOAD_PORT);
    if(lwip_bind(s_srv,(struct sockaddr*)&a,sizeof(a))<0){lwip_close(s_srv);s_srv=-1;goto idle;}
    if(lwip_listen(s_srv,1)<0){lwip_close(s_srv);s_srv=-1;goto idle;}

    for(;;){
        cl=sizeof(c);
        r=lwip_accept(s_srv,(struct sockaddr*)&c,&cl);
        if(r<0){vTaskDelay(pdMS_TO_TICKS(200));continue;}
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
