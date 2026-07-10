# User 源码文件索引（自动生成）

> 由 `node tools/generate_file_index.js` 扫描 `@file` / `@brief` 生成。

| 路径 | 功能说明 |
| --- | --- |
| `bsp/bsp_msg.c` | BSP 消息队列/事件辅助（通用模板，DSO 主链路未直接使用）。 |
| `bsp/bsp_msg.h` | BSP 消息模块头文件。 |
| `bsp/bsp.c` | 板级支持包入口：MPU、I/D Cache、400MHz 时钟、HAL_Init、基础外设初始化。 |
| `bsp/bsp.h` | BSP 公共头：System_Init、bsp_Init 及外设驱动 include 聚合。 |
| `bsp/inc/bsp_beep.h` | 安富莱 BSP 通用驱动（蜂鸣器）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_camera.h` | 安富莱 BSP 通用驱动（摄像头）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_can.h` | 安富莱 BSP 通用驱动（CAN 总线）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_cpu_adc.h` | 安富莱 BSP 通用驱动（CPU 内置 ADC）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_cpu_dac.h` | 安富莱 BSP 通用驱动（CPU 内置 DAC）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_cpu_flash.h` | 安富莱 BSP 通用驱动（内部 Flash）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_cpu_rtc.h` | 安富莱 BSP 通用驱动（RTC 实时时钟）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_dht11.h` | 安富莱 BSP 通用驱动（DHT11 温湿度）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ds18b20.h` | 安富莱 BSP 通用驱动（DS18B20 温度）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_dwt.h` | DWT 周期计数器：性能 profiling 与 RenderLatch 耗时统计。 |
| `bsp/inc/bsp_eth.h` | 安富莱 BSP 通用驱动（以太网）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_fmc_ad7606.h` | 安富莱 BSP 通用驱动（AD7606 FMC ADC）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_fmc_io.h` | 安富莱 BSP 通用驱动（FMC 扩展 IO）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_fmc_nand_flash.h` | 安富莱 BSP 通用驱动（NAND Flash）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_fmc_oled.h` | 安富莱 BSP 通用驱动（FMC OLED）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_fmc_sdram.h` | FMC 外部 SDRAM 驱动：32MB @0xC0000000 初始化与测试 API。 |
| `bsp/inc/bsp_i2c_bh1750.h` | 安富莱 BSP 通用驱动（BH1750 光照）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_bmp085.h` | 安富莱 BSP 通用驱动（BMP085 气压）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_eeprom_24xx.h` | 安富莱 BSP 通用驱动（24xx EEPROM）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_gpio.h` | 安富莱 BSP 通用驱动（软件 I2C GPIO）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_hmc5883l.h` | 安富莱 BSP 通用驱动（HMC5883L 磁力计）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_mpu6050.h` | 安富莱 BSP 通用驱动（MPU6050 IMU）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_si4730.h` | 安富莱 BSP 通用驱动（Si4730 收音机）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_i2c_wm8978.h` | 安富莱 BSP 通用驱动（WM8978 音频 CODEC）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ir_decode.h` | 安富莱 BSP 通用驱动（红外解码）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_key.h` | 安富莱 BSP 通用驱动（按键）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_lcd_rgb.h` | RGB LCD / LTDC 层配置、背光、帧缓冲地址接口。 |
| `bsp/inc/bsp_led.h` | 板载 LED GPIO 驱动头文件。 |
| `bsp/inc/bsp_mg323.h` | 安富莱 BSP 通用驱动（MG323 模块）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ps2.h` | 安富莱 BSP 通用驱动（PS/2 键盘）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_qspi_w25q256.h` | 安富莱 BSP 通用驱动（W25Q256 QSPI Flash）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_rs485_led.h` | 安富莱 BSP 通用驱动（RS485 LED）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_sdio_sd.h` | This file contains the common defines and functions prototypes for |
| `bsp/inc/bsp_sdram_layout.h` | SDRAM 应用区划分：GUI heap、Record buffer、RollHistory 等。 |
| `bsp/inc/bsp_spi_ad9833.h` | 安富莱 BSP 通用驱动（AD9833 DDS）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_ads1256.h` | 安富莱 BSP 通用驱动（ADS1256 ADC）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_bus.h` | 安富莱 BSP 通用驱动（SPI 总线）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_dac8501.h` | 安富莱 BSP 通用驱动（DAC8501）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_dac8562.h` | 安富莱 BSP 通用驱动（DAC8562）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_flash.h` | 安富莱 BSP 通用驱动（SPI Flash）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_tm7705.h` | 安富莱 BSP 通用驱动（TM7705 ADC）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_spi_vs1053b.h` | 安富莱 BSP 通用驱动（VS1053B 音频）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_tft_429.h` | 安富莱 BSP 通用驱动（F429 TFT）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_tft_h7.h` | 安富莱 BSP 通用驱动（H7 TFT）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_tft_lcd.h` | 安富莱 BSP 通用驱动（TFT LCD 通用）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_tim_pwm.h` | 安富莱 BSP 通用驱动（TIM PWM）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_timer.h` | 安富莱 BSP 通用驱动（通用定时器）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ts_ft5x06.h` | 安富莱 BSP 通用驱动（FT5x06 触摸）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ts_gt811.h` | 安富莱 BSP 通用驱动（GT811 触摸）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ts_gt911.h` | 安富莱 BSP 通用驱动（GT911 触摸）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ts_stmpe811.h` | 安富莱 BSP 通用驱动（STMPE811 触摸）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ts_touch.h` | 安富莱 BSP 通用驱动（触摸通用）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_uart_esp8266.h` | 安富莱 BSP 通用驱动（ESP8266 WiFi）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_uart_fifo.h` | UART FIFO 驱动：printf、VoFA、串口诊断底层。 |
| `bsp/inc/bsp_uart_gps.h` | 安富莱 BSP 通用驱动（GPS 模块）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_uart_sim800.h` | 安富莱 BSP 通用驱动（SIM800 蜂窝）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_ui_io_expander.h` | MCP23017 I2C IO 扩展：EC1/EC2 编码器 A/B/R 与按键。 |
| `bsp/inc/bsp_user_lib.h` | 安富莱 BSP 通用驱动（BSP 用户工具函数）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/bsp_vram_layout.h` | 显存布局单一来源：逻辑横屏与物理竖屏 ping/pong 地址。 |
| `bsp/inc/bsp_watchdog.h` | 安富莱 BSP 通用驱动（独立看门狗）头文件；当前 DSO 主链路未启用。 |
| `bsp/inc/debug_uart_lock.h` | 串口 printf 互斥锁：多任务并发日志时防交错。 |
| `bsp/inc/dma2d_nlr.h` | DMA2D NLR 寄存器打包辅助（宽高 → NLR 域）。 |
| `bsp/inc/dma2d_wait.h` | DMA2D 硬件忙等待与超时辅助 API。 |
| `bsp/inc/stm32h7xx_nucleo_144.h` | This file contains definitions for: |
| `bsp/inc/vram_sentinels.h` | 物理帧缓冲后方哨兵：检测 SDRAM 显示/DMA2D 越界。 |
| `bsp/src/bsp_beep.c` | 安富莱 BSP 通用驱动（蜂鸣器）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_camera.c` | 安富莱 BSP 通用驱动（摄像头）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_can.c` | 安富莱 BSP 通用驱动（CAN 总线）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_cpu_adc.c` | 安富莱 BSP 通用驱动（CPU 内置 ADC）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_cpu_dac.c` | 安富莱 BSP 通用驱动（CPU 内置 DAC）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_cpu_flash.c` | 安富莱 BSP 通用驱动（内部 Flash）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_cpu_rtc.c` | 安富莱 BSP 通用驱动（RTC 实时时钟）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_dht11.c` | 安富莱 BSP 通用驱动（DHT11 温湿度）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ds18b20.c` | 安富莱 BSP 通用驱动（DS18B20 温度）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_dwt.c` | DWT 初始化与读周期 API。 |
| `bsp/src/bsp_fmc_ad7606.c` | 安富莱 BSP 通用驱动（AD7606 FMC ADC）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_fmc_io.c` | 安富莱 BSP 通用驱动（FMC 扩展 IO）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_fmc_nand_flash.c` | 安富莱 BSP 通用驱动（NAND Flash）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_fmc_oled.c` | 安富莱 BSP 通用驱动（FMC OLED）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_fmc_sdram.c` | SDRAM 时序配置、读写自检、GUI/Record 堆基址依赖。 |
| `bsp/src/bsp_i2c_bh1750.c` | 安富莱 BSP 通用驱动（BH1750 光照）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_bmp085.c` | 安富莱 BSP 通用驱动（BMP085 气压）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_eeprom_24xx_keil.c` | 安富莱 BSP 24xx EEPROM Keil 专用实现；DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_eeprom_24xx.c` | 安富莱 BSP 通用驱动（24xx EEPROM）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_gpio_keil.c` | 安富莱 BSP 软件 I2C GPIO Keil 专用实现；DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_gpio.c` | 安富莱 BSP 通用驱动（软件 I2C GPIO）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_hmc5883l.c` | 安富莱 BSP 通用驱动（HMC5883L 磁力计）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_mpu6050.c` | 安富莱 BSP 通用驱动（MPU6050 IMU）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_si4730.c` | 安富莱 BSP 通用驱动（Si4730 收音机）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_i2c_wm8978.c` | 安富莱 BSP 通用驱动（WM8978 音频 CODEC）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ir_decode.c` | 安富莱 BSP 通用驱动（红外解码）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_key.c` | 安富莱 BSP 通用驱动（按键）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_lcd_rgb.c` | LTDC GPIO、时序、Layer1 初始化与行中断。 |
| `bsp/src/bsp_led.c` | LED1-3 初始化；PB15/LED4 默认关闭（TIM12 测频占用）。 |
| `bsp/src/bsp_ps2.c` | 安富莱 BSP 通用驱动（PS/2 键盘）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_qspi_w25q256.c` | 安富莱 BSP 通用驱动（W25Q256 QSPI Flash）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_rs485_led.c` | 安富莱 BSP 通用驱动（RS485 LED）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_sdio_sd.c` | This file includes the uSD card driver mounted on STM32H743I-EVAL |
| `bsp/src/bsp_spi_ad9833.c` | 安富莱 BSP 通用驱动（AD9833 DDS）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_ads1256.c` | 安富莱 BSP 通用驱动（ADS1256 ADC）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_bus.c` | 安富莱 BSP 通用驱动（SPI 总线）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_dac8501.c` | 安富莱 BSP 通用驱动（DAC8501）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_dac8562.c` | 安富莱 BSP 通用驱动（DAC8562）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_flash.c` | 安富莱 BSP 通用驱动（SPI Flash）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_tm7705.c` | 安富莱 BSP 通用驱动（TM7705 ADC）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_spi_vs1053b.c` | 安富莱 BSP 通用驱动（VS1053B 音频）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_tft_429.c` | 安富莱 BSP 通用驱动（F429 TFT）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_tft_h7.c` | 安富莱 BSP 通用驱动（H7 TFT）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_tft_lcd.c` | 安富莱 BSP 通用驱动（TFT LCD 通用）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_tim_pwm.c` | 安富莱 BSP 通用驱动（TIM PWM）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_timer.c` | 安富莱 BSP 通用驱动（通用定时器）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ts_ft5x06.c` | 安富莱 BSP 通用驱动（FT5x06 触摸）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ts_gt811.c` | 安富莱 BSP 通用驱动（GT811 触摸）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ts_gt911.c` | 安富莱 BSP 通用驱动（GT911 触摸）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ts_stmpe811.c` | 安富莱 BSP 通用驱动（STMPE811 触摸）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ts_touch.c` | 安富莱 BSP 通用驱动（触摸通用）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_uart_esp8266.c` | 安富莱 BSP 通用驱动（ESP8266 WiFi）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_uart_fifo.c` | COM1 FIFO 收发与中断服务。 |
| `bsp/src/bsp_uart_gps.c` | 安富莱 BSP 通用驱动（GPS 模块）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_uart_sim800.c` | 安富莱 BSP 通用驱动（SIM800 蜂窝）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_ui_io_expander.c` | IO 扩展器读写与去抖采样。 |
| `bsp/src/bsp_user_lib.c` | 安富莱 BSP 通用驱动（BSP 用户工具函数）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/bsp_watchdog.c` | 安富莱 BSP 通用驱动（独立看门狗）驱动实现；当前 DSO 主链路未启用。 |
| `bsp/src/debug_uart_lock.c` | DebugUartLock/Unlock 实现。 |
| `bsp/src/dma2d_wait.c` | DMA2D 等待实现，供 PlotBuf 与 emWin 填色同步。 |
| `bsp/src/vram_sentinels.c` | 哨兵初始化、周期校验与失败计数。 |
| `bsp/stm32h7xx_hal_conf.h` | HAL configuration template file. |
| `bsp/stm32h7xx_it.c` | Main Interrupt Service Routines. |
| `bsp/stm32h7xx_it.h` | This file contains the headers of the interrupt handlers. |
| `bsp/y_t_stress_ab.h` | Y-T 显示/引擎场测 A/B 开关集合（单变量对比用）。 |
| `dso_integration.h` | DSO 引擎集成开关：USE_DSO_ENGINE 选择真实采集或假数据任务。 |
| `dso/dso_10bit_display_sync.c` | 10-bit 显示同步实现。 |
| `dso/dso_10bit_display_sync.h` | CH1/CH2 触发位置按采样率映射；HS10 限 CH1。 |
| `dso/dso_10bit_lane_cal.c` | 10-bit lane cal 求解与 Apply。 |
| `dso/dso_10bit_lane_cal.h` | 10-bit CH1 双 lane 校准：offset/gain/skew，EEPROM 持久化。 |
| `dso/dso_acq_10bit.c` | 10-bit HAL 实现：CH1 双 lane 交织、CH2 实时 ADC3。 |
| `dso/dso_acq_10bit.h` | 10-bit 采集后端 API：ADC1/2/3 + TIM1 + DMA 配置。 |
| `dso/dso_acq_8bit.c` | 8-bit HAL 实现：tri-interleaved，CH2 不可用。 |
| `dso/dso_acq_8bit.h` | 8-bit 高速采集后端 API：三路 ADC 交替采样。 |
| `dso/dso_acq_common.c` | 采集公共逻辑：环形缓冲辅助、运行时信息。 |
| `dso/dso_acq_common.h` | 8-bit/10-bit 后端公共辅助声明。 |
| `dso/dso_acq_hal_stm32h7.c` | HAL IRQ 桥接：DMA half/full、ADC 回调 → DSO_Acq facade。 |
| `dso/dso_acq_internal.h` | 采集子模块内部共享类型与常量。 |
| `dso/dso_acq_model.h` | ADC/TIM 时钟模型：估算 lane 采样率与 TIM ARR。 |
| `dso/dso_acq.c` | 采集模式状态机、format 切换 pending、后端路由。 |
| `dso/dso_acq.h` | 采集 facade：统一 start/stop/pos/buf API，分发 8-bit/10-bit 后端。 |
| `dso/dso_acq10_stream.c` | 10-bit stream 合并实现。 |
| `dso/dso_acq10_stream.h` | 10-bit 双 lane 合并与 ring-copy 纯 C helper（可 host 测试）。 |
| `dso/dso_acq10_tim_trigger.c` | 10-bit TIM trigger 配置实现。 |
| `dso/dso_acq10_tim_trigger.h` | 10-bit TIM1 触发模式选择（PWM1 等）。 |
| `dso/dso_acq8_fault_policy.c` | 8-bit fault 检测与恢复策略。 |
| `dso/dso_acq8_fault_policy.h` | 8-bit 采集故障策略（auto fallback 等，默认关闭）。 |
| `dso/dso_acq8_refresh_policy.c` | 8-bit refresh 策略实现。 |
| `dso/dso_acq8_refresh_policy.h` | 8-bit preview 刷新策略。 |
| `dso/dso_acq8_sample_unpack.c` | 8-bit unpack 与多 lane 拷贝实现。 |
| `dso/dso_acq8_sample_unpack.h` | 8-bit 样本解包、lane 检测与 copy helper。 |
| `dso/dso_acq8_tim_trigger.c` | 8-bit TIM trigger 实现。 |
| `dso/dso_acq8_tim_trigger.h` | 8-bit TIM 触发模式选择。 |
| `dso/dso_acquire_control.c` | AcquireCtrl pending 状态与 run_requested 逻辑暂停。 |
| `dso/dso_acquire_control.h` | 采集控制：RUN/STOP、触发模式/源/边沿/电平、记录深度。 |
| `dso/dso_cache_range.c` | cache range 对齐与 clean/invalidate 封装。 |
| `dso/dso_cache_range.h` | D-Cache 地址范围 32B 对齐辅助。 |
| `dso/dso_config.h` | DSO 全局编译配置：ADC 参数、显示尺寸、调试开关、默认档位等。 |
| `dso/dso_engine_cal.c` | 引擎校准 Apply 与 HS8 重入策略。 |
| `dso/dso_engine_cal.h` | 引擎校准策略：HS8/HS10 lane cal 与 topology 变更处理。 |
| `dso/dso_engine_roll.c` | Roll 引擎逻辑实现。 |
| `dso/dso_engine_roll.h` | Roll 模式引擎分支：ring feed 与 roll render 协调。 |
| `dso/dso_engine.c` | DSO_Engine_Task：采集服务、ingest、触发、时基渲染、UI 帧发布。 |
| `dso/dso_engine.h` | DSO 主引擎任务 API 与 g_dso_engine_task 句柄。 |
| `dso/dso_freq_meter_hw_tim12.c` | TIM12 IRQ、input capture、external clock 实现。 |
| `dso/dso_freq_meter_hw_tim12.h` | TIM12_CH2/PB15 硬件捕获与 gated count。 |
| `dso/dso_freq_meter_math.c` | freq meter math 实现。 |
| `dso/dso_freq_meter_math.h` | 频率计纯数学：period/gate、overflow 扩展、模式选择。 |
| `dso/dso_freq_meter.c` | FreqMeter Init/Start/Service 与帧字段更新。 |
| `dso/dso_freq_meter.h` | PB15/TIM12 外部频率计状态机 API。 |
| `dso/dso_hs10_render.c` | HS10 render 实现。 |
| `dso/dso_hs10_render.h` | 10-bit 高频重复信号增强重建（128 phase bins）。 |
| `dso/dso_hs8_lane_cal.c` | HS8 lane cal 实现。 |
| `dso/dso_hs8_lane_cal.h` | 8-bit 三路 lane 校准。 |
| `dso/dso_hs8_render.c` | HS8 render 实现。 |
| `dso/dso_hs8_render.h` | 8-bit 高频重复信号增强重建。 |
| `dso/dso_lane_merge.c` | lane merge 算法实现。 |
| `dso/dso_lane_merge.h` | 多 lane common-delta 对齐合并原语。 |
| `dso/dso_process.c` | Process 分析算法与 DSO_Frame 测量字段填充。 |
| `dso/dso_process.h` | 原始样本分析：触发点、频率、Vpp/Vrms/Duty、重复性置信度。 |
| `dso/dso_record.c` | Record ingest、single capture freeze、view 窗口管理。 |
| `dso/dso_record.h` | 深存储 record ring：pretrigger、view window、CH1/CH2。 |
| `dso/dso_render_policy.c` | render policy 实现。 |
| `dso/dso_render_policy.h` | 渲染重建策略决策（envelope/line/HS 增强）。 |
| `dso/dso_roll_line_builder.c` | RollLineBuilder 状态机与像素列聚合。 |
| `dso/dso_roll_line_builder.h` | 从 RollHistory ring 构造 decimated 波形线。 |
| `dso/dso_roll_physics.c` | roll physics 实现。 |
| `dso/dso_roll_physics.h` | Roll 模式 samples-per-pixel 等物理量计算。 |
| `dso/dso_sample_spec.c` | 采样格式表与默认上电格式（8-bit high-speed）。 |
| `dso/dso_sample_spec.h` | 产品采样格式规格：8-bit 三路交替与 10-bit 双 lane 定义。 |
| `dso/dso_serial_debug_format.c` | 格式化实现。 |
| `dso/dso_serial_debug_format.h` | 串口调试输出格式化声明。 |
| `dso/dso_serial_debug_math.h` | 串口调试数学辅助 inline/声明。 |
| `dso/dso_serial_debug.c` | 串口调试主逻辑与命令解析。 |
| `dso/dso_serial_debug.h` | 结构化串口调试：snapshot、raw dump、frame log。 |
| `dso/dso_testgen.c` | TestGen 启动与样本注入。 |
| `dso/dso_testgen.h` | 内部测试信号源（与 Wavegen 编译互斥）。 |
| `dso/dso_timebase.c` | 时基 Apply、RenderEx、RollRender 实现。 |
| `dso/dso_timebase.h` | 时基表、Normal/Fast/Roll/ETS 渲染、窗口样本数。 |
| `dso/dso_trigger_source_bridge.c` | UI 触发源 → DSO_AcquireCtrl_SetTriggerSource 桥接。 |
| `dso/dso_ui_bridge.c` | UI bridge 辅助函数与帧元数据默认值。 |
| `dso/dso_ui_bridge.h` | DSO_UI_Frame 类型与引擎→UI 帧字段、ADC LSB 换算 inline。 |
| `dso/dso_vertical.c` | 垂直档位与 ADC→plot Y 映射实现。 |
| `dso/dso_vertical.h` | 垂直通道：V/div、offset、probe、coupling、MapY。 |
| `dso/dso_vofa_firewater.c` | VoFA 协议打包与发送。 |
| `dso/dso_vofa_firewater.h` | VoFA+ FireWater 波形串口输出。 |
| `dso/dso_wavegen_dac_pa4.c` | DAC PA4 HAL 驱动与启动/停止。 |
| `dso/dso_wavegen_dac_pa4.h` | PA4 DAC1 硬件输出配置。 |
| `dso/dso_wavegen_lut.c` | LUT 数据与插值。 |
| `dso/dso_wavegen_lut.h` | 波形 DAC 查找表。 |
| `dso/dso_wavegen_timer_math.c` | timer math 实现。 |
| `dso/dso_wavegen_timer_math.h` | 波形发生器 TIM/DAC 周期计算。 |
| `dso/dso_wavegen_types.h` | 波形发生器类型：方波/正弦等 profile 枚举。 |
| `dso/dso_wavegen.c` | Wavegen FreeRTOS 任务与 profile 调度。 |
| `dso/dso_wavegen.h` | PA4 DAC 波形发生器任务与 profile 管理。 |
| `dso/roll_history_mem.c` | RollHistory 内存初始化。 |
| `dso/roll_history_mem.h` | SDRAM RollHistory 内存区划分与基址。 |
| `dso/roll_history_ring.c` | Roll ring push/copy/seq 实现。 |
| `dso/roll_history_ring.h` | RollHistory CH1/CH2 环形缓冲操作 API。 |
| `FreeRTOSConfig.h` | FreeRTOS 内核配置：堆大小、优先级、钩子、运行时统计等。 |
| `gui_heap_experiment.h` | emWin GUI 堆 SDRAM 区域实验配置（与 bsp_sdram_layout 配合）。 |
| `includes.h` | 工程全局头文件聚合：HAL、FreeRTOS、BSP、GUI 等公共 include。 |
| `main.c` | 固件入口：System_Init、SDRAM/LTDC 初始化、FreeRTOS 任务创建与调度器启动。 |
| `rtos_cpu_stats.c` | FreeRTOS 运行时 CPU 占用统计：TIM6 时基与统计任务。 |
| `rtos_cpu_stats.h` | RTOS CPU 统计 API 与开关声明。 |
| `segger/HardFaultHandlerIAR/SEGGER_HardFaultHandler.c` | SEGGER HardFault 处理（IAR 版）。 |
| `segger/HardFaultHandlerMDK/SEGGER_HardFaultHandler.c` | SEGGER HardFault 处理（Keil MDK 版）：寄存器 dump 与栈回溯。 |
| `stm32h7xx_hal_timebase_tim.c` | HAL 时基：SysTick 替代 TIM 实现，配合 FreeRTOS。 |
| `SysInfoTest.c` | 系统信息/堆栈测试辅助（非主链路，调试用）。 |
| `ui/dma2d_wave.c` | PlotBuf 波形位图：AXI SRAM 竖屏向缓冲 + DMA2D 画线/R2M，与 MainTask 波形区配合。 |
| `ui/dma2d_wave.h` | PlotBuf 波形 DMA2D 路径的公共 API、坐标映射与 CW/CCW 旋转编译开关。 |
| `ui/lcd_rotate_profile.c` | LCD 旋转 profiling 累计与串口输出。 |
| `ui/lcd_rotate_profile.h` | LCD 旋转各阶段 DWT profiling 宏。 |
| `ui/lcd_rotate_request.h` | RenderLatch 管线 API：Prepare/Commit、dirty 标记、ManualRotate。 |
| `ui/MainTask_profile.h` | MainTask 分段耗时统计宏（mtp_acc/mtp_tick）。 |
| `ui/MainTask.c` | DSO 示波器 UI 主循环：emWin 绘制、PlotBuf/DMA2D 波形、逻辑→物理旋转提交。 |
| `ui/MainTask.h` | emWin 主 UI 任务入口（FreeRTOS 任务名 "emWin"）。 |
| `ui/profile_opts.h` | MainTask/LCD 旋转 profiling 编译选项聚合。 |
| `ui/roll_history_ui.c` | Roll 时基切换时的 UI 预填：从 SDRAM ring 拉样本经 RollLineBuilder 生成整屏 y_coords。 |
| `ui/roll_history_ui.h` | Roll 模式 UI 侧辅助：时基切换后的环缓冲预填与最后一帧快照。 |
| `ui/ui_app_config.h` | UI 层编译期配置：帧率、Demo 模式、PlotBuf A/B、EC 轮询、诊断开关。 |
| `ui/ui_channel_ctrl.c` | 逻辑通道 CH1/CH2 显示开关（默认 CH1=ON、CH2=OFF）。 |
| `ui/ui_channel_ctrl.h` | 逻辑通道 CH1/CH2 显示开关（与 Solo、MainTask 波形可见性组合使用）。 |
| `ui/ui_display_light_path.h` | 轻显示路径编译约束：PlotBuf + Header 静态跳过。 |
| `ui/ui_ec_poll_task.c` | 编码器/按键轮询 FreeRTOS 任务实现。 |
| `ui/ui_ec_poll_task.h` | 编码器/按键独立轮询任务，与 MainTask 渲染环解耦。 |
| `ui/ui_freq_format.c` | 外部测频/周期字符串格式化实现。 |
| `ui/ui_freq_format.h` | PB15/TIM12 外部测频与 CH1 周期显示的字符串格式化（测量页/头栏复用）。 |
| `ui/ui_measure_units.h` | 测量页/游标：ADC 码差 → 电压的 inline 换算（复用 dso_ui_bridge 帧元数据）。 |
| `ui/ui_nav.c` | 页面导航与 EC1/EC2 编码器输入路由（由 UI_EcPollTask 周期调用 UI_Nav_Poll）。 |
| `ui/ui_nav.h` | 页面状态机与输入事件 API（Poll 在 UI_EcPollTask，Draw 在 MainTask）。 |
| `ui/ui_perf_baseline.h` | 场测性能基线版本号与默认帧周期。 |
| `ui/ui_screens.c` | 欢迎页、主菜单、通道/触发/测量/系统设置等子页面的 emWin 绘制实现。 |
| `ui/ui_screens.h` | 非示波器子页面的屏幕 ID 与绘制/生命周期钩子。 |
| `ui/ui_system_setup.c` | 系统页状态与 8-bit(3L) / 10-bit 采集格式切换逻辑。 |
| `ui/ui_system_setup.h` | 系统设置页：ADC 8-bit/10-bit 格式切换与维护项焦点。 |
| `ui/ui_theme.h` | 全应用 UI 配色与布局常量（子页与 MainTask 示波器页对齐）。 |
| `ui/ui_trigger_ui.c` | 触发设置页的 UI 镜像与绘制；选项通过 UI_TrigUi_PushAcquireBackedFields 写入 DSO pending。 |
| `ui/ui_trigger_ui.h` | 触发页 UI 镜像：源/边沿/模式/电平 preset 与 DSO pending 对接。 |
| `ui/UI_WaveformCtrl.c` | 实现 UI_WaveformCtrl.h：2-slot 帧环、DSO_Frame→DSO_UI_Frame 转换、旋钮/按键参数路由。 |
| `ui/UI_WaveformCtrl.h` | UI 与 DSO 引擎之间的波形/参数桥接层（帧环 + 用户操作路由）。 |

共 **270** 个文件。
