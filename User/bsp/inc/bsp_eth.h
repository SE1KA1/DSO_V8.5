/**
 * @file bsp_eth.h
 * @brief 安富莱 BSP 通用驱动（以太网）头文件；当前 DSO 主链路未启用。
 */

/*
*********************************************************************************************************
*
*	ģ������ : STM32F4 ����ETH MAC ����ģ��
*	�ļ����� : bsp_eth.h
*	��    �� : V2.4
*	˵    �� : ������STM32-V5������������MACΪ DM9161
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BSP_ETH_H
/* 宏定义: __BSP_ETH_H - 头文件保护宏。 */
#define __BSP_ETH_H

#ifdef __cplusplus
 extern "C" {
#endif

/* ���� ETH_PHY �жϿ��� PH6  used to manage Ethernet link status */
#define ETH_LINK_EXTI_LINE             EXTI_Line6
/* 宏定义: ETH_LINK_EXTI_PORT_SOURCE - ETH_LINK_EXTI_PORT_SOURCE 的宏定义。 */
#define ETH_LINK_EXTI_PORT_SOURCE      EXTI_PortSourceGPIOH
/* 宏定义: ETH_LINK_EXTI_PIN_SOURCE - ETH_LINK_EXTI_PIN_SOURCE 的宏定义。 */
#define ETH_LINK_EXTI_PIN_SOURCE       EXTI_PinSource6
#define ETH_LINK_EXTI_IRQn             EXTI9_5_IRQn			/* �жϺţ���stm32f4xx.h �ж��� */

#define ETH_LINK_PIN                   GPIO_Pin_6
/* 宏定义: ETH_LINK_GPIO_PORT - ETH_LINK_GPIO_PORT 的宏定义。 */
#define ETH_LINK_GPIO_PORT             GPIOH
/* 宏定义: ETH_LINK_GPIO_CLK - ETH_LINK_GPIO_CLK 的宏定义。 */
#define ETH_LINK_GPIO_CLK              RCC_AHB1Periph_GPIOH


#define PHY_ADDRESS      0x00 	/* ��ַ��DM9161��RXD0 - RXD3 �������ϵ縴λʱ����. ������STM32-V5�������ַ��0 */


/* PHY registers */
#define PHY_MICR                  0x11 	/* MII Interrupt Control Register */
#define PHY_MICR_INT_EN           ((uint16_t)0x0002) /* PHY Enable interrupts */
#define PHY_MICR_INT_OE           ((uint16_t)0x0001) /* PHY Enable output interrupt events */
#define PHY_MISR                  0x12 /* MII Interrupt Status and Misc. Control Register */
#define PHY_MISR_LINK_INT_EN      ((uint16_t)0x0020) /* Enable Interrupt on change of link status */
#define PHY_LINK_STATUS           ((uint16_t)0x2000) /* PHY link status interrupt mask */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void  ETH_BSP_Config(void);
uint32_t Eth_Link_PHYITConfig(uint16_t PHYAddress);
void Eth_Link_EXTIConfig(void);
void Eth_Link_ITHandler(uint16_t PHYAddress);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4x7_ETH_BSP_H */


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
