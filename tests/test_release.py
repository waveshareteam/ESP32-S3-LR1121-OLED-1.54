"""Exercise release integrity and publication gates with synthetic firmware."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "releases"))
import package_firmware as pack
import release_firmware as release

SHA = "a" * 40
ENTRY = {"name": "01_i2c_scan", "path": "examples/esp-idf/01_i2c_scan", "target": "esp32s3",
         "framework": "esp-idf", "version": "v5.5.5"}


def firmware(body=b"application"):
    header = bytearray(24)
    header[0], header[12] = 0xE9, 9
    return bytes(header) + body


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.build = self.base / "build"
        self.build.mkdir()
        (self.build / "boot.bin").write_bytes(firmware(b"boot"))
        (self.build / "app.bin").write_bytes(firmware())
        (self.build / "flasher_args.json").write_text(json.dumps({
            "flash_files": {"0x10000": "app.bin", "0x0": "boot.bin"},
            "flash_settings": {"flash_mode": "dio", "flash_freq": "80m", "flash_size": "16MB"}}))
        self.output = self.base / "out"
        self.archive = pack.package("esp-idf", ENTRY, ENTRY["version"], self.build, self.output, SHA)

    def rewrite(self, change):
        with zipfile.ZipFile(self.archive) as z:
            infos = z.infolist()
            data = {i.filename: z.read(i) for i in infos}
        change(data)
        with zipfile.ZipFile(self.archive, "w") as z:
            for info in infos:
                z.writestr(info, data[info.filename])

    def test_repeatable_archive_and_complete_set(self):
        original = self.archive.read_bytes()
        pack.package("esp-idf", ENTRY, ENTRY["version"], self.build, self.output, SHA)
        self.assertEqual(original, self.archive.read_bytes())
        manifest = release.validate_archive(self.archive, ENTRY, SHA)
        self.assertEqual(manifest["combined"]["size"], 0x10000 + len(firmware()))
        with patch.object(release, "expected_entries", return_value=[ENTRY]):
            self.assertEqual(release.validate_set(self.output, SHA), [self.archive])

    def test_missing_duplicate_extra_and_stale_artifacts_fail(self):
        with self.assertRaisesRegex(ValueError, "matrix mismatch"):
            release.validate_set(self.output, SHA)
        with patch.object(release, "expected_entries", return_value=[ENTRY]):
            copy = self.output / "nested"
            copy.mkdir()
            shutil.copy(self.archive, copy / self.archive.name)
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                release.validate_set(self.output, SHA)
            (copy / self.archive.name).rename(copy / "unexpected.zip")
            with self.assertRaisesRegex(ValueError, "matrix mismatch"):
                release.validate_set(self.output, SHA)
        with self.assertRaisesRegex(ValueError, "filename"):
            release.validate_archive(self.archive, ENTRY, "b" * 40)

    def test_corrupt_binary_is_rejected(self):
        self.rewrite(lambda data: data.__setitem__("bin/app.bin", firmware(b"corrupt")))
        with self.assertRaisesRegex(ValueError, "digest or size"):
            release.validate_archive(self.archive, ENTRY, SHA)

    def test_unsafe_and_duplicate_members_fail(self):
        with zipfile.ZipFile(self.archive, "a") as z:
            z.writestr("../outside.bin", b"unsafe")
        with self.assertRaisesRegex(ValueError, "Unsafe ZIP path"):
            release.validate_archive(self.archive, ENTRY, SHA)
        self.archive = pack.package("esp-idf", ENTRY, ENTRY["version"], self.build, self.output, SHA)
        import warnings
        with warnings.catch_warnings(), zipfile.ZipFile(self.archive, "a") as z:
            warnings.simplefilter("ignore", UserWarning)
            z.writestr("manifest.json", b"{}")
        with self.assertRaisesRegex(ValueError, "Duplicate ZIP member"):
            release.validate_archive(self.archive, ENTRY, SHA)

    def test_stale_manifest_commit_is_rejected(self):
        def stale(data):
            m = json.loads(data["manifest.json"])
            m["git_sha"] = "b" * 40
            data["manifest.json"] = json.dumps(m).encode()
        self.rewrite(stale)
        with self.assertRaisesRegex(ValueError, "git_sha"):
            release.validate_archive(self.archive, ENTRY, SHA)

    def test_padding_rejected_even_with_updated_hashes(self):
        def pad(data):
            data["bin/combined.bin"] += b"\xff" * 128
            m = json.loads(data["manifest.json"])
            m["combined"]["size"] += 128
            m["combined"]["sha256"] = hashlib.sha256(data["bin/combined.bin"]).hexdigest()
            data["manifest.json"] = json.dumps(m).encode()
        self.rewrite(pad)
        with self.assertRaisesRegex(ValueError, "appended image padding"):
            release.validate_archive(self.archive, ENTRY, SHA)

    def test_overlaps_wrong_chip_and_missing_bootloader_fail(self):
        (self.build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x0": "boot.bin", "0x10": "app.bin"}}))
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            pack.package("esp-idf", ENTRY, ENTRY["version"], self.build, self.output, SHA)
        (self.build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x10000": "app.bin"}}))
        with self.assertRaisesRegex(ValueError, "bootloader"):
            pack.package("esp-idf", ENTRY, ENTRY["version"], self.build, self.output, SHA)
        (self.build / "app.bin").write_bytes(b"invalid chip image" * 3)
        with self.assertRaisesRegex(ValueError, "ESP32-S3"):
            pack.package("esp-idf", ENTRY, ENTRY["version"], self.build, self.output, SHA)

    def test_arduino_merged_fallback_is_stored_once(self):
        entry = dict(ENTRY, framework="arduino", version="3.3.12", path="examples/arduino/01_i2c_scan")
        merged = firmware() + b"\xff" * 1000
        (self.build / "sketch.ino.merged.bin").write_bytes(merged)
        path = pack.package("arduino", entry, entry["version"], self.build, self.output, SHA)
        release.validate_archive(path, entry, SHA)
        with zipfile.ZipFile(path) as z:
            self.assertEqual([n for n in z.namelist() if n.endswith(".bin")], ["bin/sketch.ino.merged.bin"])

    def test_posix_flash_helper_handles_spaces_and_fallback(self):
        with zipfile.ZipFile(self.archive) as z:
            script = self.base / "flash.sh"
            script.write_bytes(z.read("flash.sh"))
            batch = z.read("flash.bat").decode()
            self.assertIn('py -3 -m esptool --chip esp32s3 --port "%PORT%"', batch)
        bindir = self.base / "commands"
        bindir.mkdir()
        shutil.copy('/usr/bin/dirname', bindir / 'dirname')
        fake = bindir / "python3"
        fake.write_text('#!/bin/sh\n[ "$1" = "-c" ] && exit 0\nprintf "%s\\n" "$@" > "$FLASH_TEST_LOG"\n')
        fake.chmod(0o755)
        log = self.base / "args"
        subprocess.run(['/bin/sh', str(script), 'port with spaces'], env={'PATH':str(bindir), 'FLASH_TEST_LOG':str(log)}, check=True)
        args = log.read_text().splitlines()
        self.assertEqual(args[args.index('--port') + 1], 'port with spaces')
        self.assertIn('@flash_args.txt', args)

    def test_public_release_is_never_overwritten(self):
        with patch.object(release, "verify_tag"), patch.object(release, "validate_set", return_value=[self.archive]), \
             patch.object(release, "api", return_value=[{"tag_name":"v1.0.0", "draft":False}]) as api, \
             patch.object(release.subprocess, "run") as run:
            with self.assertRaisesRegex(ValueError, "published Release"):
                release.publish(self.output, SHA, "v1.0.0", "owner/repo", "main")
            self.assertEqual(api.call_count, 1)
            run.assert_not_called()

    def test_remote_digest_mismatch_keeps_release_draft(self):
        current = {"id": 1, "tag_name": "v1.0.0", "draft": True, "assets": []}
        uploaded = dict(current, assets=[{"name":self.archive.name,"state":"uploaded","size":self.archive.stat().st_size,
                                          "digest":"sha256:" + "0" * 64}])
        with patch.object(release, "verify_tag"), patch.object(release, "validate_set", return_value=[self.archive]), \
             patch.object(release, "verify_remote_tag"), \
             patch.object(release, "api", side_effect=[[current], current, uploaded]) as api, \
             patch.object(release.subprocess, "run"):
            with self.assertRaisesRegex(ValueError, "digest mismatch"):
                release.publish(self.output, SHA, "v1.0.0", "owner/repo", "main")
            self.assertTrue(all(len(call.args) < 3 or call.args[2] != 'PATCH' for call in api.call_args_list))

    def test_annotated_remote_tag_must_resolve_to_source_commit(self):
        with patch.object(release, "api", side_effect=[{"object":{"type":"tag", "sha":"c"*40}},
                                                       {"object":{"type":"commit", "sha":SHA}}]):
            release.verify_remote_tag("owner/repo", "v1.0.0", SHA)
        with patch.object(release, "api", return_value={"object":{"type":"commit", "sha":"b"*40}}):
            with self.assertRaisesRegex(ValueError, "tag target changed"):
                release.verify_remote_tag("owner/repo", "v1.0.0", SHA)

    def test_verified_draft_publishes_then_rechecks_public_download(self):
        current = {"id":1, "tag_name":"v1.0.0", "draft":True, "assets":[]}
        uploaded = dict(current, assets=[{"name":self.archive.name,"state":"uploaded",
            "size":self.archive.stat().st_size,
            "digest":"sha256:" + hashlib.sha256(self.archive.read_bytes()).hexdigest()}])
        public = dict(uploaded, draft=False, prerelease=False, html_url="https://github.com/owner/repo/releases/tag/v1.0.0")
        def command(args, **kwargs):
            if args[:3] == ["gh", "release", "download"]:
                shutil.copy(self.archive, Path(args[args.index("--dir") + 1]) / self.archive.name)
        with patch.object(release, "verify_tag"), patch.object(release, "verify_remote_tag"), \
             patch.object(release, "expected_entries", return_value=[ENTRY]), \
             patch.object(release, "api", side_effect=[[], current, current, uploaded, public]) as api, \
             patch.object(release.subprocess, "run", side_effect=command):
            release.publish(self.output, SHA, "v1.0.0", "owner/repo", "main")
            self.assertEqual(api.call_args_list[-1].args[2], "PATCH")
            self.assertEqual(api.call_args_list[-1].args[3], {"draft":False, "make_latest":"true"})


if __name__ == "__main__":
    unittest.main()
