#include "dso_eth_upload.h"
#include "dso_switch_cmd.h"
#include "lwip.h"
#include "ethernetif.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/errno.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

#define ETH_UPLOAD_PORT 5005
#define UDP_TEST_PORT   5006

static int s_cli=-1, s_srv=-1;
static SemaphoreHandle_t s_mtx;

/* 命令行缓冲：跨 lwip_recv 分片积累，遇 \n 触发一行分发 */
static char s_cmd_line[512];
static int  s_cmd_line_len;

/* ---- 大包回显模式（吞吐测试）：0xE0 0xA5 | <len:4 小端> | <payload len 字节> → 原样回显 ---- */
#define ETH_ECHO_MAGIC0     0xE0u
#define ETH_ECHO_MAGIC1     0xA5u
#define ETH_ECHO_MAX_LEN    (64u * 1024u * 1024u)   /* 64MB 防呆上限 */

typedef enum {
    ETH_ECHO_IDLE = 0,
    ETH_ECHO_MAGIC0_SEEN,
    ETH_ECHO_HEADER,
    ETH_ECHO_STREAM
} EthEchoState;

static EthEchoState s_echo_state = ETH_ECHO_IDLE;
static uint32_t     s_echo_remain = 0;
static uint8_t      s_echo_hdr[4];
static uint8_t      s_echo_hdr_idx = 0;
static uint32_t     s_echo_count = 0;

/* 发送全部字节（lwip_send 可能部分发送——窗口满时返回 < 请求数，必须循环发满） */
static int echo_send_all(int fd, const uint8_t *data, int len)
{
    int off = 0;
    while (off < len) {
        int n = lwip_send(fd, &data[off], (size_t)(len - off), 0);
        if (n <= 0) return -1;   /* n==0 无进展 → 中止，防死循环挂死 */
        off += n;
    }
    return 0;
}

/* 行式命令字节处理（供回显状态机回退调用）。
 * 应答缓冲为**文件级 static**：授权类命令的应答可达数百字节（签名 128 + 公钥 128
 * + DeviceID），放栈上会吃掉任务栈。本文件只有一个命令处理任务，无重入。 */
static char s_reply[DSO_SWITCH_CMD_REPLY_MAX];
#define ETH_LINE_BYTE(c) do { \
    if ((c) == '\n') { \
        char *reply = s_reply; \
        s_cmd_line[s_cmd_line_len] = '\0'; \
        if (DSO_Switch_Cmd_HandleLine(s_cmd_line, reply, sizeof(s_reply))) { \
            printf("[SWC] %s -> %s\r\n", s_cmd_line, reply); \
            DSO_ETH_Upload_SendLine(reply); \
        } else { \
            DSO_ETH_Upload_SendLine(s_cmd_line); \
        } \
        s_cmd_line_len = 0; \
    } else if ((c) != '\r') { \
        if (s_cmd_line_len < (int)sizeof(s_cmd_line) - 1) s_cmd_line[s_cmd_line_len++] = (uint8_t)(c); \
    } \
} while (0)

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
    printf("[ETH] task enter\r\n");
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
    /* 链路诊断：立即查 PHY 应答与 REF_CLK；3s 后（自动协商期）再查 link 状态 */
    ETH_DebugDumpStatus();
    vTaskDelay(pdMS_TO_TICKS(3000));
    ETH_DebugDumpStatus();
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* UDP test: send to PC */
//    {
//        struct sockaddr_in dst;
//        memset(&dst,0,sizeof(dst));
//        dst.sin_family=AF_INET;
//        dst.sin_addr.s_addr = PP_HTONL(0xC0A86464U); /* 192.168.100.100 */
//        dst.sin_port = lwip_htons(5005);
//        s_srv=lwip_socket(AF_INET,SOCK_DGRAM,0);
//        printf("[ETH] UDP socket=%d\r\n",s_srv);
//        const char *msg="DSO_UDP_TEST";
//        for(int i=0;i<10;i++){
//            int n=lwip_sendto(s_srv,msg,strlen(msg),0,(struct sockaddr*)&dst,sizeof(dst));
//            printf("[ETH] UDP send#%d ret=%d errno=%d\r\n",i,n,errno);
//            vTaskDelay(pdMS_TO_TICKS(2000));
//        }
//        lwip_close(s_srv);
//    }
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
        printf("[ETH] client connected\r\n");
        s_cmd_line_len = 0;
        s_echo_state = ETH_ECHO_IDLE;
        s_echo_remain = 0;
        s_echo_hdr_idx = 0;
        s_echo_count = 0;
        {
          char buf[2048]; int n;
          int echo_err = 0;
          while(!echo_err && (n=lwip_recv(s_cli,buf,sizeof(buf),0))>0){
            int i = 0;
            while(i < n){
              uint8_t b = (uint8_t)buf[i];

              /* 回显流：整段 bulk 回显，边收边回（发送必须发满，防部分发送丢字节） */
              if(s_echo_state == ETH_ECHO_STREAM){
                uint32_t take = ((uint32_t)(n - i) < s_echo_remain) ? (uint32_t)(n - i) : s_echo_remain;
                if(take > 0){
                  if(echo_send_all(s_cli, (const uint8_t *)&buf[i], (int)take) < 0){ echo_err = 1; break; }
                  i += (int)take;
                  s_echo_remain -= take;
                }
                if(s_echo_remain == 0){
                  s_echo_state = ETH_ECHO_IDLE;
                  /* 诊断：每 25 包打印一次剩余堆，观察内存是否随测试下降 */
                  if((++s_echo_count % 25) == 0){
                    printf("[ETH] echo#%lu heap_free=%lu\r\n",
                           (unsigned long)s_echo_count,
                           (unsigned long)xPortGetFreeHeapSize());
                  }
                }
                continue;
              }

              /* 回显头：收 4 字节 len（小端），回显 magic+len 后进流 */
              if(s_echo_state == ETH_ECHO_HEADER){
                s_echo_hdr[s_echo_hdr_idx++] = b;
                if(s_echo_hdr_idx == 4){
                  static const uint8_t echo_magic[2] = {ETH_ECHO_MAGIC0, ETH_ECHO_MAGIC1};
                  uint32_t len = (uint32_t)s_echo_hdr[0]
                               | ((uint32_t)s_echo_hdr[1] << 8)
                               | ((uint32_t)s_echo_hdr[2] << 16)
                               | ((uint32_t)s_echo_hdr[3] << 24);
                  if(echo_send_all(s_cli, echo_magic, 2) < 0 ||
                     echo_send_all(s_cli, s_echo_hdr, 4) < 0){ echo_err = 1; break; }
                  s_echo_remain = (len <= ETH_ECHO_MAX_LEN) ? len : 0;
                  s_echo_state = ETH_ECHO_STREAM;
                  if(s_echo_remain == 0) s_echo_state = ETH_ECHO_IDLE;
                }
                i++;
                continue;
              }

              /* 已见 0xE0：下字节是 0xA5 → 进头；否则 0xE0 按行式处理 */
              if(s_echo_state == ETH_ECHO_MAGIC0_SEEN){
                if(b == ETH_ECHO_MAGIC1){
                  s_echo_state = ETH_ECHO_HEADER;
                  s_echo_hdr_idx = 0;
                  i++;
                  continue;
                }
                s_echo_state = ETH_ECHO_IDLE;
                ETH_LINE_BYTE(ETH_ECHO_MAGIC0);
                /* fall through：当前字节 b 继续行式 */
              } else if(b == ETH_ECHO_MAGIC0){
                s_echo_state = ETH_ECHO_MAGIC0_SEEN;
                i++;
                continue;
              }

              /* 行式命令处理 */
              ETH_LINE_BYTE(b);
              i++;
            }
          }
        }
        printf("[ETH] client disconnected\r\n");
        lwip_close(s_cli); s_cli=-1; vTaskDelay(pdMS_TO_TICKS(100));
    }
idle: for(;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

/* ---- 裸 UDP 带宽测试接收端（PC→STM32，端口 5006）---- */

void DSO_ETH_UDP_RX_Task(void *pv)
{
    struct sockaddr_in laddr;
    struct sockaddr_in raddr;
    socklen_t rl;
    char buf[1472];
    int s;
    uint32_t count = 0;
    uint32_t bytes = 0;
    uint32_t last_status = 0;
    (void)pv;

    /* 等待 LwIP netif up */
    while (netif_default == NULL || !netif_is_link_up(netif_default)) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    s = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        printf("[UDP] socket fail errno=%d\r\n", errno);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family      = AF_INET;
    laddr.sin_addr.s_addr = INADDR_ANY;
    laddr.sin_port        = lwip_htons(UDP_TEST_PORT);
    if (lwip_bind(s, (struct sockaddr *)&laddr, sizeof(laddr)) < 0) {
        printf("[UDP] bind %d fail errno=%d\r\n", UDP_TEST_PORT, errno);
        lwip_close(s);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("[UDP] RX listening :%d\r\n", UDP_TEST_PORT);

    rl = sizeof(raddr);
    for (;;) {
        int n = lwip_recvfrom(s, buf, sizeof(buf), 0,
                              (struct sockaddr *)&raddr, &rl);
        if (n > 0) {
            /* RST: 复位计数器（每用例开始前由 PC 发送）；GET: 立即回当前计数 */
            if (n >= 3 && memcmp(buf, "RST", 3) == 0) {
                count = 0;
                bytes = 0;
            } else if (n >= 3 && memcmp(buf, "GET", 3) == 0) {
                char sbuf[40];
                int sn = snprintf(sbuf, sizeof(sbuf), "UDP:%lu:%lu\r\n",
                                  (unsigned long)count, (unsigned long)bytes);
                (void)lwip_sendto(s, sbuf, (size_t)sn, 0,
                                  (struct sockaddr *)&raddr, sizeof(raddr));
            } else {
                count++;
                bytes += (uint32_t)n;
            }
        }
        /* 每 1s 向最后发送方回传已收计数（PC 据此算丢包） */
        if ((HAL_GetTick() - last_status) >= 1000) {
            char sbuf[40];
            int sn;
            last_status = HAL_GetTick();
            sn = snprintf(sbuf, sizeof(sbuf), "UDP:%lu:%lu\r\n",
                          (unsigned long)count, (unsigned long)bytes);
            (void)lwip_sendto(s, sbuf, (size_t)sn, 0,
                              (struct sockaddr *)&raddr, sizeof(raddr));
        }
    }
}

void DSO_ETH_UDP_RX_Init(void)
{
    if (xTaskCreate(DSO_ETH_UDP_RX_Task, "UDP_RX", 1024, NULL, 1, NULL) != pdPASS) {
        printf("[BOOT] FAIL UDP_RX\r\n");
    }
}
