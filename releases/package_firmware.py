#!/usr/bin/env python3
"""Package real source-build outputs; never package factory binaries implicitly."""
import argparse
from datetime import datetime, timezone
import os
import re
import shlex
import stat
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from discover_examples import ROOT, discover, select, settings


def files_for_build(framework, build):
    if framework == "esp-idf":
        metadata = json.loads((build / "flasher_args.json").read_text())
        pairs = list(metadata["flash_files"].items())
        flash_settings = metadata.get("flash_settings", {})
        options = []
        for key in ("flash_mode", "flash_freq", "flash_size"):
            if key in flash_settings:
                options.extend(["--" + key.replace("_", "-"), str(flash_settings[key])])
    else:
        merged = sorted(build.glob("*.merged.bin"))
        if len(merged) != 1:
            raise ValueError("Expected exactly one Arduino merged binary; export it with the pinned core")
        pairs, options = [("0x0", merged[0].name)], []
    if not pairs:
        raise ValueError("Empty flash file list")
    names, output = set(), []
    for offset, filename in pairs:
        source = (build / filename).resolve()
        if not source.is_relative_to(build.resolve()) or not source.is_file():
            raise ValueError("Flash input is missing or escapes the build directory")
        if source.name in names or not source.stat().st_size:
            raise ValueError("Duplicate or empty flash input")
        int(offset, 0)
        names.add(source.name)
        output.append((offset, source))
    return options, output


FLASH_LIMIT = 16 * 1024 * 1024


def image_header(payload):
    # ESP32-S3 image header: magic 0xe9, chip ID 9 at bytes 12..13.
    if len(payload) < 24 or payload[0] != 0xE9 or int.from_bytes(payload[12:14], "little") != 9:
        raise ValueError("Expected an ESP32-S3 firmware image")


def source_epoch():
    value = os.environ.get("SOURCE_DATE_EPOCH")
    if value is None:
        value = subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=ROOT, text=True).strip()
    return int(value)


def package(framework, example, version, build, output, git_sha, framework_ref=None):
    if not re.fullmatch(r"[0-9a-f]{40}", git_sha) or example["target"] != "esp32s3":
        raise ValueError("Expected a full source commit and the ESP32-S3 target")
    options, files = files_for_build(framework, build)
    epoch = source_epoch()
    manifest = {"schema": 2, "name": example["name"], "framework": framework,
                "framework_version": version, "target": example["target"],
                "project_path": example["path"], "git_sha": git_sha,
                "timestamp_utc": datetime.fromtimestamp(epoch, timezone.utc).isoformat(),
                "baud": 460800, "flash_options": options, "files": []}
    if framework_ref:
        manifest["framework_ref"] = framework_ref
    payloads = {}
    combined = bytearray()
    for offset, source in sorted(files, key=lambda pair: int(pair[0], 0)):
        start = int(offset, 0)
        data = source.read_bytes()
        if start < len(combined) or start + len(data) > FLASH_LIMIT:
            raise ValueError("Overlapping flash segments or image exceeds 16 MB")
        if start in (0, 0x10000):
            image_header(data)
        combined.extend(b"\xff" * (start - len(combined)))
        combined.extend(data)
        path = "bin/" + source.name
        if path == "bin/combined.bin" and framework == "esp-idf":
            raise ValueError("Input filename conflicts with the combined image")
        payloads[path] = data
        manifest["files"].append({"offset": hex(start), "path": path, "size": len(data),
                                  "sha256": hashlib.sha256(data).hexdigest()})
    if not manifest["files"] or manifest["files"][0]["offset"] != "0x0":
        raise ValueError("A flashable package must include the bootloader at offset zero")
    if framework == "esp-idf":
        combined_path = "bin/combined.bin"
        payloads[combined_path] = bytes(combined)
    else:
        # The core's merged export includes boot_app0 and all required segments.
        # Keep that canonical image once when separate boot_app0 is unavailable.
        combined_path = manifest["files"][0]["path"]
    manifest["combined"] = {"path": combined_path, "offset": "0x0", "size": len(combined),
                            "sha256": hashlib.sha256(combined).hexdigest()}
    args = [*options, "0x0", combined_path]
    manifest["flash_command"] = ["python", "-m", "esptool", "--chip", "esp32s3",
                                 "--port", "<PORT>", "--baud", "460800", "write-flash", "@flash_args.txt"]
    payloads["manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    payloads["flash_args.txt"] = ("\n".join(shlex.quote(arg) for arg in args) + "\n").encode()
    payloads["flash.sh"] = b'''#!/bin/sh
set -eu
cd "$(dirname "$0")"
[ "$#" -eq 1 ] || { echo "Usage: sh flash.sh PORT" >&2; exit 2; }
if python -c 'import esptool' >/dev/null 2>&1; then
    PYTHON=python
elif python3 -c 'import esptool' >/dev/null 2>&1; then
    PYTHON=python3
else
    echo "Install esptool: python3 -m pip install 'esptool>=5,<6'" >&2
    exit 1
fi
exec "$PYTHON" -m esptool --chip esp32s3 --port "$1" --baud 460800 write-flash @flash_args.txt
'''
    payloads["flash.bat"] = '''@echo off
setlocal
cd /d "%~dp0"
if "%~1"=="" exit /b 2
set "PORT=%~1"
python -c "import esptool" >nul 2>&1
if not errorlevel 1 goto python
py -3 -c "import esptool" >nul 2>&1
if not errorlevel 1 goto launcher
echo Install esptool: py -3 -m pip install "esptool>=5,<6"
exit /b 1
:python
python -m esptool --chip esp32s3 --port "%PORT%" --baud 460800 write-flash @flash_args.txt
exit /b %errorlevel%
:launcher
py -3 -m esptool --chip esp32s3 --port "%PORT%" --baud 460800 write-flash @flash_args.txt
exit /b %errorlevel%
'''.replace("\n", "\r\n").encode()
    payloads["README.md"] = (f"# {example['name']}\n\n"
        f"Framework: {framework} {version}. Target: esp32s3. Source: {git_sha}.\n\n"
        "Install esptool 5.x and close serial monitors. Flashing replaces device firmware.\n\n"
        "Ubuntu Bash: `sh flash.sh /dev/ttyACM0`\n\n"
        "Windows PowerShell: `.\\flash.bat COM3`\n\n"
        "The combined image is flashed at offset zero. IDF packages also retain original segments.\n"
        "Arduino packages retain the core's canonical merged export, including its flash padding.\n"
        "Build success does not certify physical-board behavior.\n").encode()
    payloads["SHA256SUMS"] = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(payloads.items())).encode()
    output.mkdir(parents=True, exist_ok=True)
    dest = output / f"{framework}-{example['name']}-{version}-{example['target']}-{git_sha[:8]}.zip"
    stamp = datetime.fromtimestamp(max(epoch, 315532800), timezone.utc).timetuple()[:6]
    with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(payloads.items()):
            info = zipfile.ZipInfo(name, stamp)
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | (0o755 if name == "flash.sh" else 0o644)) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    return dest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--framework", choices=["esp-idf", "arduino"], required=True)
    parser.add_argument("--selector", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "releases/dist")
    args = parser.parse_args()
    cfg = settings()
    development = cfg["esp-idf-development"] if args.framework == "esp-idf" else []
    refs = {v["version"]: v["ref"] for v in development}
    if args.version not in cfg[args.framework] and args.version not in refs:
        parser.error("Version is not in the verified framework matrix")
    examples = select(discover(args.framework), args.selector)
    sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    for example in examples:
        print(package(args.framework, example, args.version, ROOT / example["path"] / "build",
                      args.output, sha, framework_ref=refs.get(args.version)))


if __name__ == "__main__":
    main()
