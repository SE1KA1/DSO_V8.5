#!/usr/bin/env node
/**
 * 为 DSO_V8.4/User/ 下全部 .c/.h 注入 @file 功能性文件头。
 * 用法: node tools/add_source_annotations.js [--dry-run]
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const USER = path.join(ROOT, 'User');
const dryRun = process.argv.includes('--dry-run');

const EXPLICIT = require('./file_annotations.json');

const BSP_NAMES = {
  bsp_beep: '蜂鸣器', bsp_camera: '摄像头', bsp_can: 'CAN 总线',
  bsp_cpu_adc: 'CPU 内置 ADC', bsp_cpu_dac: 'CPU 内置 DAC',
  bsp_cpu_flash: '内部 Flash', bsp_cpu_rtc: 'RTC 实时时钟',
  bsp_dht11: 'DHT11 温湿度', bsp_ds18b20: 'DS18B20 温度',
  bsp_eth: '以太网', bsp_fmc_ad7606: 'AD7606 FMC ADC',
  bsp_fmc_io: 'FMC 扩展 IO', bsp_fmc_nand_flash: 'NAND Flash',
  bsp_fmc_oled: 'FMC OLED', bsp_i2c_bh1750: 'BH1750 光照',
  bsp_i2c_bmp085: 'BMP085 气压', bsp_i2c_eeprom_24xx: '24xx EEPROM',
  bsp_i2c_gpio: '软件 I2C GPIO', bsp_i2c_hmc5883l: 'HMC5883L 磁力计',
  bsp_i2c_mpu6050: 'MPU6050 IMU', bsp_i2c_si4730: 'Si4730 收音机',
  bsp_i2c_wm8978: 'WM8978 音频 CODEC', bsp_ir_decode: '红外解码',
  bsp_key: '按键', bsp_mg323: 'MG323 模块', bsp_ps2: 'PS/2 键盘',
  bsp_qspi_w25q256: 'W25Q256 QSPI Flash', bsp_rs485_led: 'RS485 LED',
  bsp_sdio_sd: 'SDIO SD 卡', bsp_spi_ad9833: 'AD9833 DDS',
  bsp_spi_ads1256: 'ADS1256 ADC', bsp_spi_bus: 'SPI 总线',
  bsp_spi_dac8501: 'DAC8501', bsp_spi_dac8562: 'DAC8562',
  bsp_spi_flash: 'SPI Flash', bsp_spi_tm7705: 'TM7705 ADC',
  bsp_spi_vs1053b: 'VS1053B 音频', bsp_tft_429: 'F429 TFT',
  bsp_tft_h7: 'H7 TFT', bsp_tft_lcd: 'TFT LCD 通用',
  bsp_tim_pwm: 'TIM PWM', bsp_timer: '通用定时器',
  bsp_ts_ft5x06: 'FT5x06 触摸', bsp_ts_gt811: 'GT811 触摸',
  bsp_ts_gt911: 'GT911 触摸', bsp_ts_stmpe811: 'STMPE811 触摸',
  bsp_ts_touch: '触摸通用', bsp_uart_esp8266: 'ESP8266 WiFi',
  bsp_uart_gps: 'GPS 模块', bsp_uart_sim800: 'SIM800 蜂窝',
  bsp_user_lib: 'BSP 用户工具函数', bsp_watchdog: '独立看门狗',
  stm32h7xx_nucleo_144: 'Nucleo-144 板级定义',
};

function describe(rel) {
  rel = rel.replace(/\\/g, '/');
  if (EXPLICIT[rel]) return EXPLICIT[rel];
  const base = path.basename(rel);
  const stem = path.basename(rel, path.extname(rel));
  if (BSP_NAMES[stem]) {
    const kind = rel.endsWith('.h') ? '头文件' : '驱动实现';
    return `安富莱 BSP 通用驱动（${BSP_NAMES[stem]}）${kind}；当前 DSO 主链路未启用。`;
  }
  if (stem.endsWith('_keil')) {
    const parent = stem.replace(/_keil$/, '');
    if (BSP_NAMES[parent]) {
      return `安富莱 BSP ${BSP_NAMES[parent]} Keil 专用实现；DSO 主链路未启用。`;
    }
  }
  if (rel.startsWith('ui/')) return `UI 模块 ${base}：示波器界面与显示链路相关代码。`;
  if (rel.startsWith('dso/')) return `DSO 核心模块 ${base}：采集/处理/时基/显示数据链路。`;
  if (rel.startsWith('bsp/')) return `BSP 模块 ${base}：STM32H743 板级支持。`;
  return `DSO V8.4 产品源码 ${base}。`;
}

function header(rel, desc) {
  const base = path.basename(rel);
  return `/**\n * @file ${base}\n * @brief ${desc}\n */\n\n`;
}

function walk(dir, out) {
  for (const ent of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, ent.name);
    if (ent.isDirectory()) walk(p, out);
    else if (ent.name.endsWith('.c') || ent.name.endsWith('.h')) out.push(p);
  }
}

const files = [];
walk(USER, files);
files.sort();

let added = 0, skip = 0;
for (const fp of files) {
  const rel = path.relative(USER, fp).replace(/\\/g, '/');
  let content = fs.readFileSync(fp, 'utf8');
  if (content.slice(0, 800).includes('@file')) { skip++; continue; }
  const h = header(rel, describe(rel));
  if (!dryRun) fs.writeFileSync(fp, h + content.replace(/^\n+/, ''), 'utf8');
  added++;
}
console.log(`[${dryRun ? 'DRY-RUN' : 'APPLIED'}] added=${added} skip=${skip}`);
