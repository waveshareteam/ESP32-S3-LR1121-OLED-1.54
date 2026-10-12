#!/usr/bin/env python3
"""Product inventory; classify source projects separately from vendor/downloads."""
import json
from pathlib import Path
from discover_examples import ROOT, discover, settings
from generate_board import board_directory


def inventory(root=ROOT):
    return {
        "first_party_idf": discover("esp-idf", root),
        "first_party_arduino": discover("arduino", root),
        "bundled_library_sketches": sorted(p.relative_to(root).as_posix()
                                            for p in (root / "libraries").rglob("*.ino")),
        "bundled_idf_examples": sorted(p.parent.relative_to(root).as_posix()
                                        for p in (root / "libraries").rglob("CMakeLists.txt")
                                        if "tools/cmake/project.cmake" in p.read_text()),
        "offline_libraries": sorted(p.name for p in (root / "libraries").iterdir() if p.is_dir()),
        "factory_binaries": sorted(p.relative_to(root).as_posix() for p in (root / "firmware").glob("*.bin")),
        "bundled_board_packs": [e["path"] + "/components/waveshare_boards" for e in discover("esp-idf", root)
                                if (root / e["path"] / "components/waveshare_boards").is_dir()],
        "local_product_boards": sorted(p.relative_to(root).as_posix() for base in ("examples/esp-idf", "firmware")
                                       for p in (root / base).glob("*/components/*/boards/" + settings(root)["board"])),
        "resolved_product_boards": {e["path"]: board_directory(e["path"], root).relative_to(root).as_posix()
                                    for e in discover("esp-idf", root)},
        "excluded_local_content": ["**/managed_components", "**/components/gen_bmgr_codes", "**/build", "**/build.*", "**/*.zip",
                                   "examples/esp-idf/08_oled_ssd1309/ESP32-S3-LR1121-OLED-1.54-*"],
    }


if __name__ == "__main__":
    print(json.dumps(inventory(), indent=2))
