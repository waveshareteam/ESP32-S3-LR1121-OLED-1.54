import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("package", Path(__file__).resolve().parents[1] / "releases/package_firmware.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackagingTests(unittest.TestCase):
    def test_idf_offsets_and_flashable_archive(self):
        with tempfile.TemporaryDirectory() as td:
            build = Path(td)
            (build / "boot.bin").write_bytes(b"boot")
            (build / "app.bin").write_bytes(b"application")
            (build / "flasher_args.json").write_text(json.dumps({
                "flash_files": {"0x0": "boot.bin", "0x10000": "app.bin"},
                "flash_settings": {"flash_mode": "dio", "flash_size": "16MB"}}))
            example = {"name": "01_demo", "path": "examples/esp-idf/01_demo", "target": "esp32s3"}
            archive = module.package("esp-idf", example, "v5.5.5", build, build / "out", "a" * 40)
            with zipfile.ZipFile(archive) as z:
                manifest = json.loads(z.read("manifest.json"))
                self.assertEqual([x["offset"] for x in manifest["files"]], ["0x0", "0x10000"])
                self.assertEqual(z.read("bin/app.bin"), b"application")
                self.assertIn("flash.bat", z.namelist())
                self.assertIn("flash.sh", z.namelist())
                self.assertIn(b"--flash-size\n16MB", z.read("flash_args"))

    def test_arduino_merged_export(self):
        with tempfile.TemporaryDirectory() as td:
            build = Path(td)
            with self.assertRaises(ValueError): module.files_for_build("arduino", build)
            (build / "demo.ino.merged.bin").write_bytes(b"merged")
            _, files = module.files_for_build("arduino", build)
            self.assertEqual(files[0][0], "0x0")

    def test_development_archive_records_framework_revision(self):
        with tempfile.TemporaryDirectory() as td:
            build = Path(td)
            (build / "app.bin").write_bytes(b"development application")
            (build / "flasher_args.json").write_text(json.dumps({"flash_files": {"0x10000": "app.bin"}}))
            example = {"name": "01_demo", "path": "examples/esp-idf/01_demo", "target": "esp32s3"}
            revision = "b" * 40
            archive = module.package("esp-idf", example, "v6.2-dev.bbbbbbbb", build,
                                     build / "out", "a" * 40, framework_ref=revision)
            with zipfile.ZipFile(archive) as z:
                manifest = json.loads(z.read("manifest.json"))
                self.assertEqual(manifest["framework_ref"], revision)
                self.assertEqual(manifest["framework_version"], "v6.2-dev.bbbbbbbb")
                self.assertIn("v6.2-dev.bbbbbbbb", archive.name)

    def test_missing_empty_and_escaping_inputs_fail(self):
        with tempfile.TemporaryDirectory() as td:
            build = Path(td)
            for files in ({}, {"0x0": "missing.bin"}, {"0x0": "../outside.bin"}):
                (build / "flasher_args.json").write_text(json.dumps({"flash_files": files}))
                with self.assertRaises(ValueError): module.files_for_build("esp-idf", build)


if __name__ == "__main__":
    unittest.main()
