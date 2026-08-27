"""
DSO error codes and error event model (matches 接口协议 v0.9 §五).

Error events are the simulator's way of reproducing the real device's
fault/exception flows. Each event carries a code, a human-readable name,
and a recoverable/irrecoverable classification.
"""

from enum import Enum
from dataclasses import dataclass


class DSOErrorCode(Enum):
    """Error codes as defined in 接口协议 v0.9 §五."""
    OVR = "E001"                 # ADC DMA overflow
    PHASE_SYNC = "E002"          # inter-lane phase sync lost
    ETH_LINK_DOWN = "E003"       # Ethernet link down
    TCP_DISCONNECT = "E004"      # TCP client disconnected
    HEAP_LOW = "E005"            # FreeRTOS heap exhausted
    FPGA_NO_RESP = "E006"        # FPGA SPI4 no response
    CAL_FAIL = "E007"            # calibration failed
    AWG_UNDERRUN = "E008"        # AWG DMA underrun
    SAFE_TRIGGERED = "E009"      # auto SAFE mode entered

    @property
    def name(self) -> str:
        _names = {
            "E001": "OVR",
            "E002": "PHASE_SYNC",
            "E003": "ETH_LINK_DOWN",
            "E004": "TCP_DISCONNECT",
            "E005": "HEAP_LOW",
            "E006": "FPGA_NO_RESP",
            "E007": "CAL_FAIL",
            "E008": "AWG_UNDERRUN",
            "E009": "SAFE_TRIGGERED",
        }
        return _names[self.value]

    @property
    def recoverable(self) -> bool:
        """Whether the error is recoverable without a reboot."""
        return self in (DSOErrorCode.ETH_LINK_DOWN,
                        DSOErrorCode.TCP_DISCONNECT,
                        DSOErrorCode.AWG_UNDERRUN,
                        DSOErrorCode.OVR,
                        DSOErrorCode.PHASE_SYNC)


@dataclass
class DSOError:
    """A single error event."""
    code: DSOErrorCode
    message: str = ""
    ts_ms: int = 0

    def to_dict(self) -> dict:
        return {"code": self.code.value, "name": self.code.name,
                "message": self.message, "ts_ms": self.ts_ms}
