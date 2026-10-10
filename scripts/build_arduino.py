#!/usr/bin/env python3
"""Build a selected product sketch using an isolated Arduino CLI cache."""
import argparse
from pathlib import Path
import subprocess
from discover_examples import ROOT, discover, select, settings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("selector")
    parser.add_argument("--prepare", action="store_true", help="Install pinned core and LVGL first")
    args = parser.parse_args()
    selected = select(discover("arduino"), args.selector)
    cfg = settings()
    cache = ROOT / ".arduino"
    cache.mkdir(exist_ok=True)
    config = cache / "arduino-cli.yaml"
    # JSON is a YAML subset; quoting remains correct even when the checkout has spaces.
    import json
    config.write_text(json.dumps({
        "board_manager": {"additional_urls": ["https://espressif.github.io/arduino-esp32/package_esp32_index.json"]},
        "directories": {"data": str(cache / "data"), "downloads": str(cache / "downloads"),
                        "user": str(cache / "user")}}, indent=2))
    cli = ["arduino-cli", "--config-file", str(config)]
    def run(*options):
        subprocess.run(cli + list(options), check=True)
    if args.prepare:
        run("core", "update-index")
        run("core", "install", "esp32:esp32@" + cfg["arduino"][0])
        run("lib", "update-index")
        run("lib", "install", "lvgl@" + cfg["lvgl"])
    for example in selected:
        flags = '-DLV_CONF_INCLUDE_SIMPLE -I"' + str(ROOT / "config/arduino") + '"'
        run("compile", "--fqbn", cfg["fqbn"], "--libraries", str(ROOT / "libraries"),
            "--build-property", "compiler.c.extra_flags=" + flags,
            "--build-property", "compiler.cpp.extra_flags=" + flags,
            "--export-binaries", "--output-dir", str(ROOT / example["path"] / "build"),
            str(ROOT / example["path"]))


if __name__ == "__main__":
    main()
