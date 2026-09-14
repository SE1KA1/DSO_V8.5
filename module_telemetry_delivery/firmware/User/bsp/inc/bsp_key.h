/**
 * @file bsp_key.h
 * @brief 安富莱 BSP 通用驱动（按键）头文件；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : ��������ģ��
*	�ļ����� : bsp_key.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_KEY_H
/* 宏定义: __BSP_KEY_H - 头文件保护宏。 */
#define __BSP_KEY_H

/* ����Ӧ�ó���Ĺ��������������� */
#define KEY_DOWN_K1		KEY_1_DOWN
/* 宏定义: KEY_UP_K1 - KEY_UP_K1 的宏定义。 */
#define KEY_UP_K1		KEY_1_UP
/* 宏定义: KEY_LONG_K1 - KEY_LONG_K1 的宏定义。 */
#define KEY_LONG_K1		KEY_1_LONG

/* 宏定义: KEY_DOWN_K2 - KEY_DOWN_K2 的宏定义。 */
#define KEY_DOWN_K2		KEY_2_DOWN
/* 宏定义: KEY_UP_K2 - KEY_UP_K2 的宏定义。 */
#define KEY_UP_K2		KEY_2_UP
/* 宏定义: KEY_LONG_K2 - KEY_LONG_K2 的宏定义。 */
#define KEY_LONG_K2		KEY_2_LONG

/* 宏定义: KEY_DOWN_K3 - KEY_DOWN_K3 的宏定义。 */
#define KEY_DOWN_K3		KEY_3_DOWN
/* 宏定义: KEY_UP_K3 - KEY_UP_K3 的宏定义。 */
#define KEY_UP_K3		KEY_3_UP
/* 宏定义: KEY_LONG_K3 - KEY_LONG_K3 的宏定义。 */
#define KEY_LONG_K3		KEY_3_LONG

#define JOY_DOWN_U		KEY_4_DOWN		/* �� */
#define JOY_UP_U		KEY_4_UP
/* 宏定义: JOY_LONG_U - JOY_LONG_U 的宏定义。 */
#define JOY_LONG_U		KEY_4_LONG

#define JOY_DOWN_D		KEY_5_DOWN		/* �� */
#define JOY_UP_D		KEY_5_UP
/* 宏定义: JOY_LONG_D - JOY_LONG_D 的宏定义。 */
#define JOY_LONG_D		KEY_5_LONG

#define JOY_DOWN_L		KEY_6_DOWN		/* �� */
#define JOY_UP_L		KEY_6_UP
/* 宏定义: JOY_LONG_L - JOY_LONG_L 的宏定义。 */
#define JOY_LONG_L		KEY_6_LONG

#define JOY_DOWN_R		KEY_7_DOWN		/* �� */
#define JOY_UP_R		KEY_7_UP
/* 宏定义: JOY_LONG_R - JOY_LONG_R 的宏定义。 */
#define JOY_LONG_R		KEY_7_LONG

#define JOY_DOWN_OK		KEY_8_DOWN		/* ok */
#define JOY_UP_OK		KEY_8_UP
/* 宏定义: JOY_LONG_OK - JOY_LONG_OK 的宏定义。 */
#define JOY_LONG_OK		KEY_8_LONG

#define SYS_DOWN_K1K2	KEY_9_DOWN		/* K1 K2 ��ϼ� */
#define SYS_UP_K1K2	    KEY_9_UP
/* 宏定义: SYS_LONG_K1K2 - SYS_LONG_K1K2 的宏定义。 */
#define SYS_LONG_K1K2	KEY_9_LONG

#define SYS_DOWN_K2K3	KEY_10_DOWN		/* K2 K3 ��ϼ� */
#define SYS_UP_K2K3  	KEY_10_UP
/* 宏定义: SYS_LONG_K2K3 - SYS_LONG_K2K3 的宏定义。 */
#define SYS_LONG_K2K3	KEY_10_LONG

/* ����ID, ��Ҫ����bsp_KeyState()��������ڲ��� */
typedef enum
{
	KID_K1 = 0,
	KID_K2,
	KID_K3,
	KID_JOY_U,
	KID_JOY_D,
	KID_JOY_L,
	KID_JOY_R,
	KID_JOY_OK
}KEY_ID_E;

/*
	�����˲�ʱ��50ms, ��λ10ms��
	ֻ��������⵽50ms״̬�������Ϊ��Ч����������Ͱ��������¼�
	��ʹ������·����Ӳ���˲������˲�����Ҳ���Ա�֤�ɿ��ؼ�⵽�����¼�
*/
#define KEY_FILTER_TIME   5
#define KEY_LONG_TIME     100			/* ��λ10ms�� ����1�룬��Ϊ�����¼� */

/*
	ÿ��������Ӧ1��ȫ�ֵĽṹ�������
*/
typedef struct
{
	/* ������һ������ָ�룬ָ���жϰ����ַ��µĺ��� */
	uint8_t (*IsKeyDownFunc)(void); /* �������µ��жϺ���,1��ʾ���� */

	uint8_t  Count;			/* �˲��������� */
	uint16_t LongCount;		/* ���������� */
	uint16_t LongTime;		/* �������³���ʱ��, 0��ʾ����ⳤ�� */
	uint8_t  State;			/* ������ǰ״̬�����»��ǵ��� */
	uint8_t  RepeatSpeed;	/* ������������ */
	uint8_t  RepeatCount;	/* �������������� */
}KEY_T;

/*
	�����ֵ����, ���밴���´���ʱÿ�����İ��¡�����ͳ����¼�

	�Ƽ�ʹ��enum, ����#define��ԭ��
	(1) ����������ֵ,�������˳��ʹ���뿴���������
	(2) �������ɰ����Ǳ����ֵ�ظ���
*/
typedef enum
{
	KEY_NONE = 0,			/* 0 ��ʾ�����¼� */

	KEY_1_DOWN,				/* 1������ */
	KEY_1_UP,				/* 1������ */
	KEY_1_LONG,				/* 1������ */

	KEY_2_DOWN,				/* 2������ */
	KEY_2_UP,				/* 2������ */
	KEY_2_LONG,				/* 2������ */

	KEY_3_DOWN,				/* 3������ */
	KEY_3_UP,				/* 3������ */
	KEY_3_LONG,				/* 3������ */

	KEY_4_DOWN,				/* 4������ */
	KEY_4_UP,				/* 4������ */
	KEY_4_LONG,				/* 4������ */

	KEY_5_DOWN,				/* 5������ */
	KEY_5_UP,				/* 5������ */
	KEY_5_LONG,				/* 5������ */

	KEY_6_DOWN,				/* 6������ */
	KEY_6_UP,				/* 6������ */
	KEY_6_LONG,				/* 6������ */

	KEY_7_DOWN,				/* 7������ */
	KEY_7_UP,				/* 7������ */
	KEY_7_LONG,				/* 7������ */

	KEY_8_DOWN,				/* 8������ */
	KEY_8_UP,				/* 8������ */
	KEY_8_LONG,				/* 8������ */

	/* ��ϼ� */
	KEY_9_DOWN,				/* 9������ */
	KEY_9_UP,				/* 9������ */
	KEY_9_LONG,				/* 9������ */

	KEY_10_DOWN,			/* 10������ */
	KEY_10_UP,				/* 10������ */
	KEY_10_LONG,			/* 10������ */
}KEY_ENUM;

/* ����FIFO�õ����� */
#define KEY_FIFO_SIZE	10
typedef struct
{
	uint8_t Buf[KEY_FIFO_SIZE];		/* ��ֵ������ */
	uint8_t Read;					/* ��������ָ��1 */
	uint8_t Write;					/* ������дָ�� */
	uint8_t Read2;					/* ��������ָ��2 */
}KEY_FIFO_T;

/* ���ⲿ���õĺ������� */
void bsp_InitKey(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_KeyScan10ms
*   功能说明: bsp_KeyScan10ms 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_KeyScan10ms(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_PutKey
*   功能说明: bsp_PutKey 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_PutKey(uint8_t _KeyCode);
/*
*********************************************************************************************************
*   函 数 名: bsp_GetKey
*   功能说明: bsp_GetKey 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t bsp_GetKey(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_GetKey2
*   功能说明: bsp_GetKey2 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t bsp_GetKey2(void);
/*
*********************************************************************************************************
*   函 数 名: bsp_GetKeyState
*   功能说明: bsp_GetKeyState 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID);
/*
*********************************************************************************************************
*   函 数 名: bsp_SetKeyParam
*   功能说明: bsp_SetKeyParam 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_SetKeyParam(uint8_t _ucKeyID, uint16_t _LongTime, uint8_t  _RepeatSpeed);
/*
*********************************************************************************************************
*   函 数 名: bsp_ClearKey
*   功能说明: bsp_ClearKey 的函数接口或内部实现。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void bsp_ClearKey(void);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
