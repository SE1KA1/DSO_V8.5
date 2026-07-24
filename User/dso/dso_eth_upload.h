#ifndef DSO_ETH_UPLOAD_H
#define DSO_ETH_UPLOAD_H
#include <stdint.h>
void DSO_ETH_Upload_Init(void);
void DSO_ETH_Upload_SendLine(const char *text);
void DSO_ETH_Upload_Task(void *pvParameters);
#endif
