#!/usr/bin/env python3
"""Update the public GPIO constants from board YAML; use idf.py bmgr for board code."""
import argparse
from pathlib import Path
import yaml
from discover_examples import ROOT, discover, settings


def board_directory(project, root=ROOT):
    return root / project / "components/esp32_s3_lr1121_oled_1_54_support/boards" / settings(root)["board"]


def hardware_header(board):
    peripherals = yaml.safe_load((board / "board_peripherals.yaml").read_text())["peripherals"]
    devices = yaml.safe_load((board / "board_devices.yaml").read_text())["devices"]
    gpios = {p["name"]: p["config"]["pin"] for p in peripherals if p["type"] == "gpio"}
    buttons = {d["name"]: d["config"]["active_level"] for d in devices if d["type"] == "button"}
    lines = ['// Generated from board YAML; see scripts/generate_board.py.', '#pragma once',
             '#include "driver/gpio.h"']
    mapping = {"BOOT_KEY_GPIO": "boot", "PWR_KEY_GPIO": "power", "MENU_KEY_GPIO": "menu",
               "PTT_KEY_GPIO": "ptt", "WS_RTC_IRQ_GPIO": "rtc_irq", "BSP_POWER_AMP_IO": "amplifier",
               "BSP_SPI_CS": "radio_cs", "BSP_SPI_INT": "radio_irq", "BSP_SPI_RESET": "radio_reset",
               "BSP_SPI_BUSY": "radio_busy"}
    for macro, name in mapping.items():
        lines.append(f'#define {macro} GPIO_NUM_{gpios["gpio_" + name]}')
    lines += [f'#define BUTTON_ACTIVE_LEVEL {buttons["button_boot"]}',
              f'#define WS_POWER_ACTIVE_LEVEL {buttons["button_power"]}']
    return "\n".join(lines) + "\n"


def generate(check=False):
    examples = discover("esp-idf")
    for example in examples:
        text = hardware_header(board_directory(example["path"]))
        path = ROOT / example["path"] / "components/esp32_s3_lr1121_oled_1_54_support/include/board_hardware.h"
        if check:
            if not path.exists() or path.read_text() != text:
                raise ValueError(f"GPIO constants are stale in {example['name']}; run python scripts/generate_board.py")
        else:
            path.write_text(text)
    print(f"GPIO constants {'match board YAML' if check else 'updated'}: {len(examples)} projects")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generate(args.check)
