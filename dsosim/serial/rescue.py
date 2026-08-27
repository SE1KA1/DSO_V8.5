"""
Rescue commands for the DSO debug UART.

The DSO firmware CLI surface is not fully frozen yet, so these presets
are sent verbatim and responses are captured for inspection. Each preset
documents its intent so it can be mapped to a real firmware command later.
"""

# name -> (description, command string to send)
RESCUE_COMMANDS = {
    "help":      ("print available commands", "help"),
    "status":    ("dump system/ETH/ADC status", "status"),
    "reset":     ("soft-reset the MCU", "reset"),
    "recover":   ("recover from SAFE state (health check)", "recover"),
    "clear_err": ("clear error counters", "clear_err"),
    "reboot":    ("full reboot (requires CONFIG/RECOVERY)", "reboot"),
    "safe":      ("enter SAFE mode explicitly", "safe"),
}


def rescue_commands() -> list[str]:
    return sorted(RESCUE_COMMANDS)
