# 模块 8：SP706TEN 看门狗（防宕机 / 远程恢复）

## 1. 硬件与用途

| 项 | 值 |
|---|---|
| 器件 | SP706TEN-L/TR（看门狗/电源监控） |
| WDI | **PB9**（原 LED3，当前固件无占用；500ms 翻转喂狗，<1.6s 超时） |
| PFO# | **PG10**（当前固件无占用；FMC16 用 NE1=PD7，不冲突） |
| 用途 | STM32H743/XO2-FPGA 防宕机：主控停机 → 看门狗复位；PFO# 指示电源故障；复位原因上报供后端判断曾否宕机；远程恢复= 下发 reboot（TBD 与后端对齐） |

## 2. 周期上报 `type: module.data`（1s 周期）

```json
"data": {
  "feed_en": 1,          // int 0/1  当前是否在喂狗（DSO_WDOG_FEED_PB9）
  "kick_count": 12345,   // int      喂狗跳变累计
  "pfo": 0,              // int      0/1 PFO# 电平；255=未读（DSO_WDOG_READ_PFO=0）
  "reset_cause": 0x20,   // int      RCC->CSR 低字节：见下表
  "uptime_ms": 99999     // int      本次运行时长（ms，约 49 天回绕）
}
```

**reset_cause 位表**（RCC->CSR 低字节）：
| 位 | 含义 |
|---|---|
| bit0 | LPWRRSTF：低功耗复位 |
| bit1 | WWDGRSTF：窗口看门狗复位 |
| bit2 | IWDGRSTF：独立看门狗复位 |
| bit3 | SFTRSTF：软件复位 |
| bit4 | PORRSTF：上电复位 |
| bit5 | PINRSTF：NRST 引脚复位 |
| bit6 | BORRSTF：欠压复位 |

（bit7=RMVF 复位标志清除位，正常为 0）

## 3. 下行命令（远程恢复）
- `reboot`（软件复位）——**已实现**（GUI 复位键，TCP 5005 简单 JSON 通道）：
  `{"cmd":"reboot","params":{"mode":"soft"},"seq":N}` → ack 后由喂狗任务延迟 ~300ms
  执行 `NVIC_SystemReset()`（先回 ack 再复位，避免应答丢失）；复位后
  `reset_cause` 的 SFTRSTF 位置位。
  - `mode:"hard"`（停喂狗 → SP706T 超时拉低 NRST，真硬件复位，~1.6s 生效）：留作后续钩子。
  - `command.execute` 通道（服务器下发）的 reboot 尚未接（见 `dso_fmc_telemetry.c` 的 tel_handle_command），需要时再补。

## 4. 示例完整帧

```json
{"ver":"1.0","type":"module.data","module":"watchdog","seq":60,"ts_ms":6200000,"data":{"feed_en":1,"kick_count":12345,"pfo":0,"reset_cause":32,"uptime_ms":99999}}
```

## 5. 后端解析要点
- `reset_cause` 建议每次上电首帧核对：IWDGRSTF(bit2) 置位 = 曾被独立看门狗复位（系统曾卡死）；PINRSTF/PORRSTF 为正常复位。
- `pfo=1`（PFO# 高/低电平以实测为准，规格初版按"0=正常"占位，联调校正）需与硬件确认语义后再定告警。
