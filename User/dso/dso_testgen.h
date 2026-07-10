/**
 * @file dso_testgen.h
 * @brief 内部测试信号源（与 Wavegen 编译互斥）。
 */

#ifndef DSO_TESTGEN_H
/* 宏定义: DSO_TESTGEN_H - 头文件保护宏。 */
#define DSO_TESTGEN_H

/*
*********************************************************************************************************
*   函 数 名: DSO_TestGen_Start
*   功能说明: 处理测试信号生成器状态、样本生成和调试输出。
*   形    参: 参见函数声明。
*   返 回 值: 参见函数声明。
*********************************************************************************************************
*/
void DSO_TestGen_Start(void);   /* Start 100KHz sine on PA4 (DAC1) */
void DSO_TestGen_Stop(void);

#endif /* DSO_TESTGEN_H */
