#!/usr/bin/env python3
"""Package real source-build outputs; never package factory binaries implicitly."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
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


def package(framework, example, version, build, output, git_sha, framework_ref=None):
    options, files = files_for_build(framework, build)
    manifest = {"name": example["name"], "framework": framework, "framework_version": version,
                "target": example["target"], "project_path": example["path"], "git_sha": git_sha,
                "timestamp_utc": datetime.now(timezone.utc).isoformat(), "baud": 460800, "files": []}
    if framework_ref:
        manifest["framework_ref"] = framework_ref
    args = options[:]
    output.mkdir(parents=True, exist_ok=True)
    dest = output / f"{framework}-{example['name']}-{version}-{example['target']}-{git_sha[:8]}.zip"
    with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as archive:
        for offset, source in files:
            path = "bin/" + source.name
            payload = source.read_bytes()
            archive.writestr(path, payload)
            manifest["files"].append({"offset": offset, "path": path, "size": len(payload),
                                      "sha256": hashlib.sha256(payload).hexdigest()})
            args.extend([offset, path])
        manifest["flash_command"] = ["python", "-m", "esptool", "--chip", example["target"],
                                      "--port", "<PORT>", "--baud", "460800", "write-flash", "@flash_args"]
        archive.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        archive.writestr("flash_args", "\n".join(args) + "\n")
        archive.writestr("flash.sh", '#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\n: "${1:?Usage: sh flash.sh PORT}"\n'
                        + f'python3 -m esptool --chip {example["target"]} --port "$1" --baud 460800 write-flash @flash_args\n')
        archive.writestr("flash.bat", '@echo off\r\ncd /d "%~dp0"\r\nif "%~1"=="" exit /b 2\r\n'
                        + f'python -m esptool --chip {example["target"]} --port "%~1" --baud 460800 write-flash @flash_args\r\n')
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
