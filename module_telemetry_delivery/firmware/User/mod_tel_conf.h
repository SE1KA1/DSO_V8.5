/**
 * @file mod_tel_conf.h
 * @brief 精简五模块上报工程 —— 网络配置点（交付同事唯一需改的文件）。
 *
 * 板子静态 IP 由 lwip.c(MX_LWIP_Init) 读取；服务器 IP/端口由
 * dso_module_upload.c 读取。改完重新编译即可。
 */
#ifndef MOD_TEL_CONF_H
#define MOD_TEL_CONF_H

/* ---- 板端静态 IP / 掩码 / 网关（同事按自己网段改） ---- */
#define MOD_TEL_BOARD_IP0     192
#define MOD_TEL_BOARD_IP1     168
#define MOD_TEL_BOARD_IP2     100
#define MOD_TEL_BOARD_IP3     50

#define MOD_TEL_NETMASK0      255
#define MOD_TEL_NETMASK1      255
#define MOD_TEL_NETMASK2      255
#define MOD_TEL_NETMASK3      0

#define MOD_TEL_GW0           192
#define MOD_TEL_GW1           168
#define MOD_TEL_GW2           100
#define MOD_TEL_GW3           1

/* ---- 服务器（数据接收端）IP/端口 ---- */
/* 宏名沿用 FMC_TEL_SERVER_*，dso_module_upload.c 无需改名 */
#define FMC_TEL_SERVER_IP0    192
#define FMC_TEL_SERVER_IP1    168
#define FMC_TEL_SERVER_IP2    100
#define FMC_TEL_SERVER_IP3    100
#define FMC_TEL_SERVER_PORT   5005u

#endif /* MOD_TEL_CONF_H */
