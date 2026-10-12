#!/usr/bin/env python3
"""Fetch the latest stable registry board profile for host checks (no firmware build)."""
import hashlib
import io
import json
from pathlib import PurePosixPath
import re
import sys
import time
import urllib.error
import urllib.request
import zipfile

from discover_examples import ROOT, settings

YAML_FILES = {"board_info.yaml", "board_devices.yaml", "board_peripherals.yaml"}
CACHE = ".ci/board-profile"


def download(url):
    if not url.startswith("https://components-file.espressif.com/") and not url.startswith("https://components.espressif.com/"):
        raise ValueError("Unexpected component registry URL")
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=45) as response:
                return response.read()
        except (urllib.error.URLError, TimeoutError):
            if attempt == 2:
                raise
            time.sleep(2 ** attempt)


def latest_release(metadata):
    versions = [v for v in metadata["versions"]
                if re.fullmatch(r"\d+\.\d+\.\d+", v["version"]) and not v.get("yanked_at")]
    if not versions:
        raise ValueError("No stable Waveshare board package is available")
    return max(versions, key=lambda v: tuple(map(int, v["version"].split("."))))


def verified_profile(archive, checksums, board):
    """Verify every published file before returning the three product YAML files."""
    if checksums["algorithm"] != "sha256":
        raise ValueError("Unsupported registry checksum algorithm")
    expected = {f["path"]: f for f in checksums["files"]}
    if len(expected) != len(checksums["files"]):
        raise ValueError("Duplicate checksum path")
    with zipfile.ZipFile(io.BytesIO(archive)) as package:
        files = [i for i in package.infolist() if not i.is_dir()]
        if len({i.filename for i in files}) != len(files):
            raise ValueError("Duplicate archive path")
        if {i.filename for i in files} != set(expected):
            raise ValueError("Archive/checksum file list mismatch")
        profile = {}
        prefix = f"boards/{board}/"
        for item in files:
            path = PurePosixPath(item.filename)
            if path.is_absolute() or ".." in path.parts or "\\" in item.filename:
                raise ValueError("Unsafe archive path")
            data = package.read(item)
            check = expected[item.filename]
            if len(data) != check["size"] or hashlib.sha256(data).hexdigest() != check["hash"]:
                raise ValueError(f"Registry checksum mismatch: {item.filename}")
            if item.filename.startswith(prefix):
                profile[item.filename[len(prefix):]] = data
        if set(profile) != YAML_FILES:
            raise ValueError(f"Expected exactly three YAML files for {board}; review the published board pack")
        return profile


def fetch(root=ROOT):
    cfg = json.loads((root / "config/waveshare_boards.json").read_text())
    if cfg["version"] != "*":
        raise ValueError("This fetcher expects the latest stable board pack (*)")
    metadata = json.loads(download(f'https://components.espressif.com/api/components/{cfg["component"]}'))
    release = latest_release(metadata)
    checksums = json.loads(download(release["checksums"]))
    profile = verified_profile(download(release["url"]), checksums, settings(root)["board"])
    destination = root / CACHE / "boards" / settings(root)["board"]
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in profile.items():
        (destination / name).write_bytes(data)
    receipt = {"component": cfg["component"], "version": release["version"],
               "component_hash": release["component_hash"], "url": release["url"],
               "files": {n: hashlib.sha256(d).hexdigest() for n, d in profile.items()}}
    (root / CACHE / "resolved.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f'Verified {cfg["component"]} {release["version"]}: {len(checksums["files"])} files; product profile cached')


if __name__ == "__main__":
    try:
        fetch()
    except (ValueError, KeyError, urllib.error.URLError) as exc:
        sys.exit(f"Board profile download failed: {exc}")
