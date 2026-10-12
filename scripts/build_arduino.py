#!/usr/bin/env python3
"""Build a selected product sketch using an isolated Arduino CLI cache."""
import argparse
import json
from pathlib import Path
import subprocess
from discover_examples import ROOT, discover, select, settings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("selector")
    parser.add_argument("--prepare", action="store_true", help="Install the pinned core first")
    parser.add_argument("--preprocess", action="store_true", help="Resolve includes without compiling firmware")
    args = parser.parse_args()
    selected = select(discover("arduino"), args.selector)
    cfg = settings()
    cache = ROOT / ".arduino"
    cache.mkdir(exist_ok=True)
    config = cache / "arduino-cli.yaml"
    # JSON is a YAML subset; quoting remains correct even when the checkout has spaces.
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
    for example in selected:
        options = []
        if example["name"] in cfg["arduino_lvgl_examples"]:
            # Arduino discovers headers before compiling. Register lv_conf.h
            # as a library so it is found in that phase too; a raw -I flag is
            # removed by the CLI's dependency-discovery preprocessing pass.
            options += ["--library", str(ROOT / "config/arduino")]
        if args.preprocess:
            options += ["--preprocess"]
        run("compile", "--fqbn", cfg["fqbn"], "--libraries", str(ROOT / "libraries"), *options,
            "--export-binaries", "--output-dir", str(ROOT / example["path"] / "build"),
            str(ROOT / example["path"]))


if __name__ == "__main__":
    main()
