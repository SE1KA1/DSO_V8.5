"""Facts the report must state: known code-vs-spec discrepancies and limits."""

from __future__ import annotations

DISCREPANCIES = [
    {
        "id": "pcal_eth_link_mislabel",
        "title": "pcal6524.eth_link is the temperature-alarm bit, not the link",
        "detail": ("The module frame computes eth_link from yds bit3 (YDS4, "
                   "temperature alarm). The true link state is only in "
                   "get_switch.data.eth_link."),
        "source": ["User/dso/dso_module_upload.c:386-391",
                   "User/dso/dso_switch_cmd.c:201-226"],
    },
    {
        "id": "ina_valid_hardcoded",
        "title": "ina226 valid is hard-coded to 1",
        "detail": ("Both INA modules send valid:1 with a TODO, so telemetry "
                   "cannot distinguish a real 0 mA from a failed sensor. U10 "
                   "returns 0 after 50 failures; U44 keeps the last value."),
        "source": ["User/dso/dso_module_upload.c:298-302",
                   "User/dso/dso_module_upload.c:322-326"],
    },
    {
        "id": "atecc_rev_type",
        "title": "atecc608b.chip_info.rev is a hex string",
        "detail": ("The spec declares rev as int; the firmware sends an "
                   "8-character hex string, and adds a device_id field not in "
                   "the spec. The reference validator ignores both."),
        "source": ["User/dso/dso_module_upload.c:245-253"],
    },
    {
        "id": "atecc_login_state_values",
        "title": "ATECC login_state uses NO_LICENSE / AUTHORIZED",
        "detail": ("The spec and reference codec enumerate LOGGED_OUT / "
                   "CHALLENGE / LOGGED_IN, but the firmware reports its own "
                   "vocabulary: NO_LICENSE (initial/after clear) and "
                   "AUTHORIZED (valid license). The harness normalizes these "
                   "two values before schema validation."),
        "source": ["User/dso/dso_auth_soft.c:48,256,445",
                   "User/dso/dso_module_upload.c:230-235"],
    },
    {
        "id": "u44_no_events",
        "title": "ina226_u44 emits no events",
        "detail": ("The codec and spec list overcurrent/recovered/comm_lost; "
                   "the firmware sends only module.data for U44."),
        "source": ["User/dso/dso_module_upload.c:316-328"],
    },
    {
        "id": "legacy_recv_storm",
        "title": "Legacy FMC telemetry connect/close storm",
        "detail": ("dso_fmc_telemetry treats any lwip_recv < 0 (including "
                   "EWOULDBLOCK) as a disconnect, so a silent server causes a "
                   "connect/close cycle at the 50 ms loop cadence. The harne"
                   "ss closes empty connections with RST so the board's 4-PCB "
                   "LwIP pool does not fill with TIME_WAIT."),
        "source": ["User/dso/dso_fmc_telemetry.c:259-282",
                   "LWIP/Target/lwipopts.h:55-60"],
    },
    {
        "id": "switch_model_only",
        "title": "Switch readback is a software model",
        "detail": ("B1-B14 are PCAL6524 outputs, not physical DIP inputs. "
                   "set_sw/get_switch/telemetry all read s_sw_bitmap; the I2C "
                   "write return code is discarded and B10 has no line."),
        "source": ["User/dso/dso_switch_panel.c:22-23,71-75,148-163"],
    },
    {
        "id": "atecc_softkey",
        "title": "ATECC authentication uses a software P-256 backend",
        "detail": ("DSO_AUTH_DEV_SOFTKEY=1 by default: auth_start/get_pubkey "
                   "use the software key, not chip ECDSA. The chip is "
                   "exercised at boot (probe/self-test) and by get_devid."),
        "source": ["User/dso/dso_config.h:333-335",
                   "User/dso/dso_switch_cmd.c:259-367"],
    },
    {
        "id": "no_switch_changed_event",
        "title": "pcal6524 switch_changed event not implemented",
        "detail": ("The optional event is in the spec/codec but the firmware "
                   "never sends it; changes are only visible in the next "
                   "1 s frame."),
        "source": ["User/dso/dso_module_upload.c:381-392"],
    },
    {
        "id": "bgm_sample_format_spec",
        "title": "bgm sample_format enum differs from the spec text",
        "detail": ("The spec describes SAMPLE/PEAK/HIRES/AVERAGE; firmware has "
                   "only 0=10-bit and 1=8-bit high-speed "
                   "(dso_sample_spec.h)."),
        "source": ["User/dso/dso_sample_spec.h:13-18",
                   "module_telemetry/packet_specs/06_bgm_adc_dac（信号采集）.md:25-26"],
    },
]

LIMITATIONS = [
    {
        "id": "alarm_levels",
        "item": "current/temperature alarm levels 1/2/3",
        "status": "N/A on the rack",
        "stimulus": ("controlled load (>+9.5 mA on U10 / +18 mA on U44) or "
                     "a heater (>58/60/65 °C)"),
    },
    {
        "id": "buzzer_rhythm",
        "item": "buzzer rhythm",
        "status": "N/A on the rack",
        "stimulus": "an alarm level or a direct buzzer test build",
    },
    {
        "id": "frequency_measurement",
        "item": "bgm frequency measurement",
        "status": "N/A on the rack",
        "stimulus": "a signal generator (or another board) on PB15/TIM12_CH2",
    },
    {
        "id": "physical_leds",
        "item": "physical LED colors / DIP line levels",
        "status": "human-assisted",
        "stimulus": "on-site person with eyes and a multimeter (see HUMAN_TODO)",
    },
    {
        "id": "watchdog",
        "item": "watchdog module",
        "status": "explicitly excluded",
        "stimulus": "out of scope for this test; frames only feed seq accounting",
    },
    {
        "id": "remote_flash",
        "item": "firmware update",
        "status": "not available",
        "stimulus": "ST-Link SWD, physical access",
    },
    {
        "id": "atecc_unlocked",
        "item": "ATECC chip lock state",
        "status": "observed on the rack chip",
        "stimulus": ("lock_config=0 and lock_data=0 (factory/unlocked); identity "
                     "and telemetry work, but chip-level provisioning/Lock is "
                     "still pending. The check reports WARN, not FAIL."),
    },
    {
        "id": "lwip_netconn_ceiling",
        "item": "board TCP resource ceiling",
        "status": "risk",
        "stimulus": ("MEMP_NUM_NETCONN=4 while listener + accepted client + "
                     "FMC telemetry + module upload + UDP-5006 = 5 sockets; "
                     "the control listener may transiently refuse. The harness "
                     "waits/retries up to control_wait_s and reports SKIP if it "
                     "stays unavailable. Close GUI/auth clients before a run."),
    },
    {
        "id": "listener_no_idle_timeout",
        "item": "board control listener has no idle timeout",
        "status": "risk",
        "stimulus": ("A control connection that dies without FIN can leave the "
                     "single-client listener blocked; if the control channel "
                     "disappears mid-run, reset the board before rerunning."),
    },
]

HUMAN_TODO = [
    {
        "id": "HUM-01",
        "title": "YDS2 green steady on",
        "instruction": "基线阶段观察 YDS2（绿）：应常亮。",
    },
    {
        "id": "HUM-02",
        "title": "YDS3 blue steady on",
        "instruction": "基线阶段观察 YDS3（蓝）：应常亮。",
    },
    {
        "id": "HUM-03",
        "title": "YDS1 red off",
        "instruction": "基线阶段观察 YDS1（红）：应熄灭。",
    },
    {
        "id": "HUM-04",
        "title": "YDS4 orange off",
        "instruction": "基线阶段观察 YDS4（橙）：应熄灭。",
    },
    {
        "id": "HUM-05",
        "title": "buzzer silent",
        "instruction": "基线阶段听蜂鸣器：应无声。",
    },
    {
        "id": "PCI-08",
        "title": "mux line levels during sweep",
        "instruction": (
            "开关扫描时用万用表量 PCAL6524 引脚：B1-B8=P0_0..P0_7，"
            "B9=P1_0，B11=P1_1，B12=P1_2，B14=P1_3，B13=P1_4；"
            "ON≈0V，OFF≈3.3V；B10 无对应线；PH7（mux G#）≈0V。"),
    },
    {
        "id": "OPT-USB",
        "title": "optional USB activity check",
        "instruction": (
            "可选：串口 1,152,000 发 {\"cmd\":\"get_switch\",\"seq\":1}，"
            "5 s 内 YDS5 应亮且遥测 usb_active 变 1。"),
    },
    {
        "id": "OPT-BOOTLOG",
        "title": "optional boot log capture",
        "instruction": (
            "可选（若允许重启）：保存重启后的串口启动日志，检查 "
            "[ATECC] selftest / [SHT40] / [INA226] / [SW] pcal init=0 / "
            "[MUPL] connected。"),
    },
]


def answers_template() -> dict:
    return {item["id"]: None for item in HUMAN_TODO
            if not item["id"].startswith("OPT-")}
