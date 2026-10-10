#!/usr/bin/env python3
"""Scan maintained first-party text; report file/rule only, never matched secrets."""
from pathlib import Path
import re
import sys
from discover_examples import ROOT, discover, settings


def maintained_files():
    files = set(ROOT.glob("*.md"))
    files.update((ROOT / "firmware").glob("*.md"))
    for name in ("docs", "config", "scripts", "tests", "releases", ".github"):
        files.update((ROOT / name).rglob("*"))
    for framework in ("esp-idf", "arduino"):
        for e in discover(framework):
            p = ROOT / e["path"]
            files.update(p.glob("*.md"))
            if framework == "arduino":
                files.update(p.glob("*.ino")); files.update(p.glob("*.cpp")); files.update(p.glob("*.h"))
            else:
                files.update((p / "main").rglob("*"))
                files.update((p / "components/esp32_s3_lr1121_oled_1_54_support").rglob("*"))
                files.update((p / "components/esp32_s3_lr1121_oled_1_54_support/boards" / settings()["board"]).rglob("*"))
                files.add(p / "CMakeLists.txt"); files.add(p / "sdkconfig.defaults")
    return sorted(p for p in files if p.is_file() and p.suffix in (".md", ".py", ".c", ".cpp", ".h", ".hpp", ".ino", ".txt", ".cmake", ".yml", ".yaml", ".json", ".defaults") and "__pycache__" not in p.parts)


def scan():
    rules = {
        "machine-specific path": r"/(?:home|Users)/[A-Za-z][\w.-]*/|[A-Za-z]:\\(?:Users|Projects)\\",
        "private-network URL": r"https?://(?:192\.168\.|10\.|172\.(?:1[6-9]|2\d|3[01])\.)",
        "private key": r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
        "credential token": r"\b(?:ghp_|github_pat_|sk-proj-)[A-Za-z0-9_]{20,}",
        "stale example path": r"04_" + "micphone|09_lora_" + "pingpong|11_" + "LoRaWAN_Starter|firmware/" + "code\b",
        "wrong product": "ESP32-S3-Touch-" + "LCD-4.3C",
    }
    issues = []
    files = maintained_files()
    for p in files:
        text = p.read_text()
        for rule, pattern in rules.items():
            if re.search(pattern, text):
                issues.append(f"{p.relative_to(ROOT)}: {rule}")
    if issues:
        raise ValueError("\n".join(issues))
    return len(files)


if __name__ == "__main__":
    try:
        print(f"Public content scan passed: {scan()} maintained text files")
    except ValueError as exc:
        sys.exit(str(exc))
