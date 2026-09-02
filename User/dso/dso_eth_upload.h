#ifndef DSO_ETH_UPLOAD_H
#define DSO_ETH_UPLOAD_H
#include <stdint.h>
void DSO_ETH_Upload_Init(void);
void DSO_ETH_Upload_SendLine(const char *text);
void DSO_ETH_Upload_Task(void *pvParameters);

/* 裸 UDP 带宽测试接收端（端口 5006）：初始化并创建 UDP_RX 任务 */
void DSO_ETH_UDP_RX_Init(void);
#endif
