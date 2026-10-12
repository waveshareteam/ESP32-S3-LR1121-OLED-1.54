#!/usr/bin/env python3
"""Validate complete source firmware sets and publish a verified draft Release."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shlex
import stat
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from discover_examples import ROOT, matrix

MAX_IMAGE = 16 * 1024 * 1024


def archive_name(entry, sha):
    return f"{entry['framework']}-{entry['name']}-{entry['version']}-{entry['target']}-{sha[:8]}.zip"


def expected_entries():
    return (matrix("esp-idf")["include"] + matrix("esp-idf", channel="development")["include"]
            + matrix("arduino")["include"])


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_image(data):
    require(len(data) >= 24 and data[0] == 0xE9 and int.from_bytes(data[12:14], "little") == 9,
            "Firmware header does not identify ESP32-S3")


def validate_archive(path, entry, sha):
    require(path.name == archive_name(entry, sha), "Noncanonical archive filename")
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        names = [i.filename for i in infos]
        require(len(names) == len(set(names)), "Duplicate ZIP member")
        for info in infos:
            name = info.filename
            p = PurePosixPath(name)
            require(not p.is_absolute() and p.as_posix() == name and ".." not in p.parts
                    and "\\" not in name and not info.is_dir(), "Unsafe ZIP path")
            require(not info.flag_bits & 1 and stat.S_ISREG(info.external_attr >> 16), "Special ZIP member")
            require(0 < info.file_size <= MAX_IMAGE, "Empty or oversized ZIP member")
        require(sum(i.file_size for i in infos) <= 2 * MAX_IMAGE + 1024 * 1024, "Oversized firmware archive")
        data = {name: archive.read(name) for name in names}
    require("manifest.json" in data, "Missing manifest")
    m = json.loads(data["manifest.json"])
    require(m.get("schema") == 2, "Unsupported manifest schema")
    for key, value in {"git_sha": sha, "name": entry["name"], "project_path": entry["path"],
                       "framework": entry["framework"], "framework_version": entry["version"],
                       "target": entry["target"], "baud": 460800}.items():
        require(m.get(key) == value, f"Manifest mismatch: {key}")
    require(m.get("framework_ref") == entry.get("ref"), "Framework revision mismatch")
    require(m["target"] == "esp32s3", "Unsupported target")
    files, combined = m["files"], m["combined"]
    require(files and combined["offset"] == "0x0", "Missing flash layout")
    paths = [f["path"] for f in files]
    require(len(paths) == len(set(paths)), "Duplicate firmware segment")
    required = {"manifest.json", "SHA256SUMS", "README.md", "flash_args.txt", "flash.sh", "flash.bat",
                combined["path"], *paths}
    require(set(data) == required, "Missing or unexpected archive members")
    for f in [*files, combined]:
        require(f["path"].startswith("bin/") and f["path"].endswith(".bin"), "Invalid firmware member")
        payload = data[f["path"]]
        require(len(payload) == f["size"] and hashlib.sha256(payload).hexdigest() == f["sha256"],
                "Firmware digest or size mismatch")
    image = data[combined["path"]]
    check_image(image)
    end = 0
    for f in files:
        offset = int(f["offset"], 0)
        require(f["offset"] == hex(offset) and offset >= end, "Overlapping or unordered segments")
        require(image[end:offset] == b"\xff" * (offset - end), "Non-erased image gap")
        payload = data[f["path"]]
        require(image[offset:offset + len(payload)] == payload, "Combined image differs from its segments")
        if offset in (0, 0x10000):
            check_image(payload)
        end = offset + len(payload)
    require(files[0]["offset"] == "0x0" and end == len(image), "Missing bootloader or appended image padding")
    if entry["framework"] == "esp-idf":
        require(combined["path"] == "bin/combined.bin" and combined["path"] not in paths,
                "Invalid IDF combined image")
        require(any(f["offset"] == "0x10000" for f in files), "Missing IDF application")
    else:
        require(len(files) == 1 and paths == [combined["path"]], "Duplicate Arduino merged image")
    options = m["flash_options"]
    require(len(options) % 2 == 0 and len(options) <= 6, "Invalid flash options")
    allowed = {"--flash-mode": {"dio", "qio", "dout", "qout", "keep"},
               "--flash-size": {"16MB", "keep", "detect"},
               "--flash-freq": {"40m", "80m", "120m", "keep"}}
    keys = options[::2]
    require(len(set(keys)) == len(keys), "Duplicate flash option")
    for key, value in zip(keys, options[1::2]):
        require(key in allowed and value in allowed[key], "Unexpected flash setting")
    require(shlex.split(data["flash_args.txt"].decode()) == [*options, "0x0", combined["path"]],
            "Flash arguments do not match the manifest")
    require(m["flash_command"] == ["python", "-m", "esptool", "--chip", "esp32s3", "--port", "<PORT>",
                                    "--baud", "460800", "write-flash", "@flash_args.txt"], "Wrong flash command")
    checksums = "".join(f"{hashlib.sha256(value).hexdigest()}  {name}\n"
                        for name, value in sorted(data.items()) if name != "SHA256SUMS")
    require(data["SHA256SUMS"].decode() == checksums, "Archive checksum inventory mismatch")
    return m


def validate_set(directory, sha):
    require(re.fullmatch(r"[0-9a-f]{40}", sha), "Expected a full source commit")
    entries = expected_entries()
    expected = {archive_name(e, sha): e for e in entries}
    require(len(expected) == len(entries), "Duplicate expected artifact name")
    actual = list(directory.rglob("*.zip"))
    names = [p.name for p in actual]
    require(len(names) == len(set(names)), "Duplicate artifact filename")
    missing, extra = set(expected) - set(names), set(names) - set(expected)
    require(not missing and not extra, f"Firmware matrix mismatch: missing={sorted(missing)}, extra={sorted(extra)}")
    for path in actual:
        validate_archive(path, expected[path.name], sha)
    return sorted(actual)


def verify_tag(tag, sha, default_branch):
    require(re.fullmatch(r"v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)", tag), "Use a stable vMAJOR.MINOR.PATCH tag")
    require(re.fullmatch(r"[0-9a-f]{40}", sha), "Expected a full commit SHA")
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()
    require(git("rev-parse", "HEAD") == sha == git("rev-parse", f"refs/tags/{tag}^{{commit}}"),
            "Tag, event commit and checkout differ")
    subprocess.run(["git", "merge-base", "--is-ancestor", sha, f"refs/remotes/origin/{default_branch}"], cwd=ROOT, check=True)


def api(repo, endpoint, method="GET", payload=None):
    args = ["gh", "api", f"repos/{repo}/{endpoint}", "--method", method]
    if payload is not None:
        args += ["--input", "-"]
    result = subprocess.run(args, input=json.dumps(payload) if payload is not None else None,
                            check=True, text=True, capture_output=True)
    return json.loads(result.stdout)


def verify_remote_tag(repo, tag, sha):
    obj = api(repo, f"git/ref/tags/{tag}")["object"]
    for _ in range(8):
        if obj["type"] != "tag":
            break
        obj = api(repo, f"git/tags/{obj['sha']}")["object"]
    require(obj["type"] == "commit" and obj["sha"] == sha, "Remote tag target changed")


def publish(directory, sha, tag, repo, default_branch):
    require(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo), "Invalid repository")
    verify_tag(tag, sha, default_branch)
    paths = validate_set(directory, sha)
    # Enumerate authenticated releases, including drafts. Never overwrite a public release.
    releases = []
    page = 1
    while True:
        items = api(repo, f"releases?per_page=100&page={page}")
        releases.extend(items)
        if len(items) < 100:
            break
        page += 1
    matches = [r for r in releases if r["tag_name"] == tag]
    require(len(matches) <= 1, "Ambiguous release tag")
    if matches:
        release = matches[0]
        require(release["draft"], "Refusing to modify a published Release; use a new version")
    else:
        verify_remote_tag(repo, tag, sha)
        body = (f"Source firmware built from `{sha}`.\n\n"
                "Includes all 11 Arduino examples and all 12 ESP-IDF source projects across the configured framework versions. "
                "ESP-IDF 6.2 development archives are experimental and include their pinned framework revision.\n\n"
                "Each ZIP contains firmware, checksums, a manifest and Windows/POSIX flash helpers. "
                "The original factory binary is excluded. Build validation does not replace hardware testing.\n")
        release = api(repo, "releases", "POST", {"tag_name": tag, "target_commitish": sha, "name": tag,
                                                "body": body, "draft": True, "prerelease": False})
    current = api(repo, f"releases/{release['id']}")
    require(current["draft"], "Release is already public")
    verify_remote_tag(repo, tag, sha)
    expected = {p.name: p for p in paths}
    require(not ({a["name"] for a in current["assets"]} - set(expected)), "Unexpected existing draft asset")
    subprocess.run(["gh", "release", "upload", tag, *map(str, paths), "--repo", repo, "--clobber"], check=True)
    current = api(repo, f"releases/{release['id']}")
    require(current["draft"], "Release changed during upload")
    assets = current["assets"]
    require(len(assets) == len(expected) and {a["name"] for a in assets} == set(expected), "Remote asset matrix mismatch")
    for asset in assets:
        payload = expected[asset["name"]].read_bytes()
        require(asset["state"] == "uploaded" and asset["size"] == len(payload)
                and asset.get("digest") == "sha256:" + hashlib.sha256(payload).hexdigest(), "Remote asset digest mismatch")
    verify_tag(tag, sha, default_branch)
    verify_remote_tag(repo, tag, sha)
    result = api(repo, f"releases/{release['id']}", "PATCH", {"draft": False, "make_latest": "true"})
    require(not result["draft"] and not result["prerelease"], "Release publication failed")
    # Independently download and revalidate one asset after publication.
    with tempfile.TemporaryDirectory() as td:
        example = expected_entries()[0]
        name = archive_name(example, sha)
        subprocess.run(["gh", "release", "download", tag, "--repo", repo, "--pattern", name, "--dir", td], check=True)
        validate_archive(Path(td) / name, example, sha)
        require((Path(td) / name).read_bytes() == expected[name].read_bytes(), "Public download differs from the validated asset")
    print(result["html_url"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["verify-tag", "validate", "publish"])
    parser.add_argument("--sha", required=True)
    parser.add_argument("--tag")
    parser.add_argument("--default-branch", default="main")
    parser.add_argument("--directory", type=Path, default=ROOT / "releases/downloads")
    parser.add_argument("--repo")
    args = parser.parse_args()
    if args.command == "verify-tag":
        verify_tag(args.tag or "", args.sha, args.default_branch)
    elif args.command == "validate":
        print(f"Verified firmware archives: {len(validate_set(args.directory, args.sha))}")
    else:
        if not args.repo or not args.tag:
            parser.error("publish requires --repo and --tag")
        publish(args.directory, args.sha, args.tag, args.repo, args.default_branch)


if __name__ == "__main__":
    main()
