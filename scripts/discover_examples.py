#!/usr/bin/env python3
"""Discover only product entry points, never recurse into libraries or backups."""
import argparse
import json
from pathlib import Path, PurePosixPath
import re

ROOT = Path(__file__).resolve().parents[1]
NAME = re.compile(r"^[0-9]{2}_[a-z0-9]+(?:_[a-z0-9]+)*$")


def settings(root=ROOT):
    return json.loads((root / "config/frameworks.json").read_text())


def discover(framework, root=ROOT):
    if framework not in ("esp-idf", "arduino"):
        raise ValueError(f"Unknown framework: {framework}")
    base = root / "examples" / framework
    marker = "CMakeLists.txt" if framework == "esp-idf" else "*.ino"
    paths = [p for p in sorted(base.iterdir()) if p.is_dir() and list(p.glob(marker))]
    if framework == "esp-idf":
        paths += [root / p for p in settings(root)["firmware_sources"]]
    found, names = [], set()
    for path in paths:
        if path.is_symlink() or not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f"Example crosses repository boundary: {path.name}")
        if not NAME.fullmatch(path.name):
            raise ValueError(f"Invalid example name: {path.name}")
        if path.name in names:
            raise ValueError(f"Duplicate example name: {path.name}")
        if framework == "arduino":
            sketches = list(path.glob("*.ino"))
            if len(sketches) != 1 or sketches[0].stem != path.name:
                raise ValueError(f"Sketch directory / .ino mismatch: {path.name}")
        elif not (path / "main/CMakeLists.txt").is_file():
            raise ValueError(f"Missing IDF project entry point: {path.name}")
        names.add(path.name)
        found.append({"name": path.name, "path": path.relative_to(root).as_posix(),
                      "target": settings(root)["target"]})
    if not found:
        raise ValueError(f"Empty {framework} example matrix")
    return found


def select(examples, selector="all"):
    if selector == "all":
        result = list(examples)
    else:
        if not selector or selector != selector.strip() or "\\" in selector:
            raise ValueError(f"Invalid selector: {selector!r}")
        path = PurePosixPath(selector)
        if path.is_absolute() or ".." in path.parts or any(c in selector for c in "*?[],$;\n"):
            raise ValueError(f"Invalid selector: {selector!r}")
        normalized = path.as_posix().rstrip("/")
        result = [e for e in examples if normalized in (e["name"], e["path"])]
    if not result:
        raise ValueError(f"Selector {selector!r} matched no examples (empty matrix)")
    if len({e["name"] for e in result}) != len(result):
        raise ValueError("Duplicate names in selected examples")
    return result


def matrix(framework, selector="all", root=ROOT, channel="stable"):
    cfg = settings(root)
    if channel == "stable":
        versions = [{"version": version} for version in cfg[framework]]
    elif channel == "development" and framework == "esp-idf":
        versions = cfg["esp-idf-development"]
        for version in versions:
            if not re.fullmatch(r"[0-9a-f]{40}", version.get("ref", "")):
                raise ValueError("Development ESP-IDF must pin a full commit SHA")
            if not re.fullmatch(r"v6\.2-dev\.[0-9a-f]{8}", version.get("version", "")):
                raise ValueError("Development ESP-IDF must be labeled v6.2-dev.<short SHA>")
            if not version["version"].endswith(version["ref"][:8]):
                raise ValueError("Development version label does not match its commit SHA")
    else:
        raise ValueError(f"Unsupported framework/channel: {framework}/{channel}")
    entries = []
    for example in select(discover(framework, root), selector):
        for version in versions:
            entry = dict(example, framework=framework, **version)
            if framework == "arduino":
                entry["fqbn"] = cfg["fqbn"]
            else:
                entry["board"] = cfg["board"]
                entry["bmgr_assist"] = cfg["bmgr_assist"]
                entry["component_manager"] = cfg["idf_component_manager"][version["version"]]
            entries.append(entry)
    if not entries:
        raise ValueError(f"No configured {channel} framework versions (empty matrix)")
    return {"include": entries}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--framework", choices=["esp-idf", "arduino"], required=True)
    parser.add_argument("--selector", default="all")
    parser.add_argument("--channel", choices=["stable", "development"], default="stable")
    args = parser.parse_args()
    try:
        print(json.dumps(matrix(args.framework, args.selector, channel=args.channel)))
    except (ValueError, FileNotFoundError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
