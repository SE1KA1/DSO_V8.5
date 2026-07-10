/*
*********************************************************************************************************
*	                                  
*	模块名称 : GUI界面主函数
*	文件名称 : MainTask.c
*	版    本 : V1.0
*	说    明 : 电阻屏四点触摸校准
*              1. 电阻屏四点触摸校准，校准后参数将保存到板载的EEPROM里面，下次上电会自动从EEPROM里面加载。
*              2. 电阻屏需要校准，电容屏无需校准。
*              3. 四点触摸校准后实现了一个简单的画板功能，用户可以检测校准是否准确，不准确就重新校准。
*              
*	修改记录 :
*		版本号   日期         作者          说明
*		V1.0    2016-07-16   Eric2013  	    首版    
*                                     
*	Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "includes.h"
#include "MainTask.h"



/*
*********************************************************************************************************
*	函 数 名: MainTask
*	功能说明: GUI主函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MainTask(void) 
{
	GUI_PID_STATE PIDState;
	
	/* 初始化 */
	GUI_Init();
	
	/*
	 关于多缓冲和窗口内存设备的设置说明
	   1. 使能多缓冲是调用的如下函数，用户要在LCDConf_Lin_Template.c文件中配置了多缓冲，调用此函数才有效：
		  WM_MULTIBUF_Enable(1);
	   2. 窗口使能使用内存设备是调用函数：WM_SetCreateFlags(WM_CF_MEMDEV);
	   3. 如果emWin的配置多缓冲和窗口内存设备都支持，二选一即可，且务必优先选择使用多缓冲，实际使用
		  STM32H7XI + 32位SDRAM + RGB565/RGB888平台测试，多缓冲可以有效的降低窗口移动或者滑动时的撕裂
		  感，并有效的提高流畅性，通过使能窗口使用内存设备是做不到的。
	   4. 所有emWin例子默认是开启三缓冲。
	*/
	WM_MULTIBUF_Enable(1);
	
	
	/* 避免上电后瞬间的撕裂感 */
	LCD_SetBackLight(0);
	GUI_SetBkColor(GUI_BLACK);
	GUI_Clear();
	GUI_Delay(200);
	LCD_SetBackLight(255);
	
	/*
       触摸校准函数默认是注释掉的，电阻屏需要校准，电容屏无需校准。如果用户需要校准电阻屏的话，执行
	   此函数即可，会将触摸校准参数保存到EEPROM里面，以后系统上电会自动从EEPROM里面加载。
	*/
    //TOUCH_Calibration();
	
	/* 
		连续读取5次，因为emWin的PID输入管理器含有一个FIFO缓冲器，
	默认情况下最多可以保存5个PID事件，下面连续读取5次相当于清空FIFO.
	防止触摸校准的时点击的点显示到画板上面。
	*/
	GUI_PID_GetState(&PIDState);
	GUI_PID_GetState(&PIDState);
	GUI_PID_GetState(&PIDState);
	GUI_PID_GetState(&PIDState);
	GUI_PID_GetState(&PIDState);
	
	/* 重定向JPEG绘制采用硬件JPEG */
	GUI_JPEG_SetpfDrawEx(JPEG_X_Draw);
	
	{
		uint32_t t0, t1, i, count = 0;
		char buf[50];
		
		/* 设置字体，文本模式和前景色 */
		GUI_SetFont(&GUI_Font24B_ASCII);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetColor(GUI_RED);	
		
		/*刷新20次，串口打印速度数值，时间单位ms */
		for(i = 0; i < 1; i++)
		{
			t0 = GUI_GetTime();
			GUI_JPEG_Draw((const void *)_ac1, sizeof(_ac1), 0, 0);
			t1 = GUI_GetTime() - t0;
			count += t1;
		}
		
		/* 求出刷新20次的平均速度 */
		sprintf(buf, "speed = %dms/frame", count/i);
		GUI_DispStringAt(buf, 10, 10);
	}
		
	/* 清屏为白色，做一个简单的画板 */
	GUI_Delay(3000);
	GUI_SetBkColor(GUI_WHITE); /* 设置背景色 */
	GUI_SetColor(GUI_BLACK);   /* 设置前景色 */
	GUI_Clear();
	GUI_SetFont(&GUI_Font24B_1);
	GUI_DispStringAt("Draw Panel", 0, 0);
	
	while(1) 
	{
		GUI_PID_GetState(&PIDState);
		if (PIDState.Pressed == 1) 
		{
			GUI_SetPenSize(5);
			/* 为了防止游标不显示或者不跟着移动，这里添加如下函数 */
			GUI_CURSOR_SetPosition(PIDState.x, PIDState.y);
			GUI_DrawPoint(PIDState.x, PIDState.y);
		}
		GUI_Delay(1);
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
