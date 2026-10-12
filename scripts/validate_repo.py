#!/usr/bin/env python3
"""Static product repository contract checks. This command never compiles firmware."""
import ast
import json
from pathlib import Path
import re
import sys
import yaml
from discover_examples import ROOT, discover, matrix, settings
from generate_board import board_directory, hardware_header


def validate(root=ROOT):
    cfg = settings(root)
    board_pack = json.loads((root / "config/waveshare_boards.json").read_text())
    assert board_pack["board"] == cfg["board"]
    assert board_pack["component"] == "waveshare/waveshare_boards"
    assert board_pack["version"] == "*", "Allow the latest compatible board-pack release"
    checks = 0
    for framework in ("esp-idf", "arduino"):
        examples = discover(framework, root)
        assert len(examples) == cfg["expected_counts"][framework], f"Inventory count changed: {framework}"
        for e in examples:
            p = root / e["path"]
            if framework == "esp-idf":
                cmake = (p / "CMakeLists.txt").read_text()
                assert f'project({e["name"]})' in cmake, f"project() mismatch: {e['path']}"
                assert "waveshare_esp_boards" not in cmake, e["path"]
                board_compat = re.search(r'include\("\$\{CMAKE_CURRENT_LIST_DIR\}/([^"\n]*esp_idf_board_compat\.cmake)"\)', cmake)
                assert board_compat and (p / board_compat[1]).resolve() == (root / "config/esp_idf_board_compat.cmake").resolve(), e["path"]
                assert board_compat.start() < cmake.index('project('), e["path"]
                defaults = (p / "sdkconfig.defaults").read_text()
                display = e["name"] in cfg["idf_display_examples"]
                assert ('CONFIG_WS_BOARD_DISPLAY=y' in defaults) == display, e["path"]
                assert 'CONFIG_IDF_TARGET="esp32s3"' in defaults, e["path"]
                assert 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y' in defaults, e["path"]
                assert 'CONFIG_ESP_CONSOLE_UART_CUSTOM=y' not in defaults, e["path"]
                assert "REQUIRES esp32_s3_lr1121_oled_1_54_support" in (p / "main/CMakeLists.txt").read_text(), e["path"]
                local = p / "components/esp32_s3_lr1121_oled_1_54_support"
                assert not (p / "components" / "ws_board").exists(), f"Duplicate support component: {e['path']}"
                local_cmake = (local / "CMakeLists.txt").read_text()
                assert "set(board_requires waveshare__waveshare_boards " in local_cmake, e["path"]
                assert "list(APPEND board_requires gen_bmgr_codes)" in local_cmake, e["path"]
                assert "WHOLE_ARCHIVE" in local_cmake, e["path"]
                assert not (p / "components/bsp_extra").exists(), e["path"]
                manifest = yaml.safe_load((p / "main/idf_component.yml").read_text())
                assert manifest["dependencies"]["idf"] == cfg["idf_version_range"], e["path"]
                assert "waveshare_esp_boards" not in manifest["dependencies"], e["path"]
                assert manifest["dependencies"][board_pack["component"]] == {"version": board_pack["version"]}, e["path"]
                assert "waveshare_boards" not in manifest["dependencies"], e["path"]
                for name in ("waveshare_boards", "waveshare__waveshare_boards"):
                    assert not (p / "components" / name).exists(), f"Local component shadows registry pack: {e['path']}"
                board = board_directory(e["path"], root)
                if e["path"].startswith("examples/"):
                    assert not (local / "boards").exists(), f"Use the online board profile: {e['path']}"
                assert {f.name for f in board.iterdir()} == {
                    "board_info.yaml", "board_devices.yaml", "board_peripherals.yaml"
                }, e["path"]
                board_info = yaml.safe_load((board / "board_info.yaml").read_text())
                assert board_info["board"] == board.name == cfg["board"], e["path"]
                for f in board.glob("*.yaml"):
                    assert yaml.safe_load(f.read_text()), str(f)
                assert not any("esp32_s3_lr1121" in d for d in manifest["dependencies"]), e["path"]
                local_deps = yaml.safe_load((local / "idf_component.yml").read_text())["dependencies"]
                assert local_deps["idf"] == cfg["idf_version_range"], e["path"]
                assert local_deps["espressif/esp_board_manager"]["version"] == cfg["board_manager"], e["path"]
                sources = {"setup_device.c"}
                headers = {"board_support.h", "board_hardware.h"}
                if e["name"] in cfg["idf_sensor_examples"]:
                    sources.add("board_sensors.c")
                if e["name"] in cfg["idf_audio_examples"]:
                    assert local_deps["chmorgan/esp-audio-player"]["version"] == cfg["audio_player"], e["path"]
                    compat = re.search(r'include\("\$\{CMAKE_CURRENT_LIST_DIR\}/([^"\n]*esp_idf_audio_compat\.cmake)"\)', cmake)
                    assert compat and (p / compat[1]).resolve() == (root / "config/esp_idf_audio_compat.cmake").resolve(), e["path"]
                    assert cmake.index('project(') < compat.start(), e["path"]
                    sources.update(("board_audio.c", "board_audio_player.c"))
                    headers.add("board_extra.h")
                if e["name"] in cfg["idf_radio_examples"]:
                    headers.update(("EspHal.h", "board_radio.h"))
                if display:
                    sources.add("board_display.c")
                    assert local_deps["espressif/esp_lvgl_adapter"]["version"] == cfg["lvgl_adapter"], e["path"]
                    assert local_deps["lvgl/lvgl"]["version"] == cfg["lvgl_component"], e["path"]
                    assert "LVGL_VERSION_MAJOR != 9" in (local / "include/board_support.h").read_text()
                assert {f.name for f in local.glob("*.c")} == sources, e["path"]
                assert {f.name for f in (local / "include").glob("*.h")} == headers, e["path"]
                for source in sources:
                    assert local_cmake.count(f'"{source}"') == 1, e["path"]
                assert (local / "include/board_hardware.h").read_text() == hardware_header(board), e["path"]
                if not display:
                    assert "CONFIG_LV_" not in defaults and "CONFIG_ESP_LV" not in defaults, e["path"]
                    assert not any("lvgl" in d for d in manifest["dependencies"]), e["path"]
                    assert not any("lvgl" in d for d in local_deps), e["path"]
                for f in (p / "main").rglob("*"):
                    if f.suffix not in (".c", ".h", ".cpp", ".hpp"):
                        continue
                    s = f.read_text()
                    for forbidden in ("i2c_new_master_bus(", "spi_bus_initialize(", "i2s_new_channel(",
                                      "iot_button_new_gpio_device(", "esp_lcd_new_panel_", "GPIO_NUM_"):
                        assert forbidden not in s, f"Hardware duplication: {f.relative_to(root)} ({forbidden})"
            if framework == "arduino" and e["name"] in cfg["arduino_lvgl_examples"]:
                sketch = (p / (e["name"] + ".ino")).read_text()
                assert "LVGL_VERSION_MAJOR != 9" in sketch
                assert "lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1)" in sketch
                assert "LV_DISPLAY_RENDER_MODE_FULL" in sketch
            checks += 1
    assert not (root / "examples/esp-idf/components").exists(), "Use each project's local board pack"
    assert cfg["idf_version_range"] == ">=5.5.5,<6.3", "IDF 6.2 must be allowed"
    development = matrix("esp-idf", root=root, channel="development")["include"]
    assert {e["path"] for e in development} == {e["path"] for e in discover("esp-idf", root)}
    for yaml_file in root.glob(".github/workflows/*.yml"):
        assert yaml.safe_load(yaml_file.read_text()), str(yaml_file)
    for p in [*root.glob("scripts/*.py"), *root.glob("tests/*.py"), *root.glob("releases/*.py")]:
        ast.parse(p.read_text(), filename=p.name)
    # libraries/lvgl is an intentional Arduino dependency. IDF display projects
    # use the managed LVGL version checked in their manifests above.
    return checks


if __name__ == "__main__":
    try:
        print(f"Repository static checks passed: {validate()} first-party entries")
    except (AssertionError, ValueError, KeyError) as exc:
        sys.exit(f"Repository validation failed: {exc}")
