import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from discover_examples import ROOT, discover, matrix, select, settings


class DiscoveryTests(unittest.TestCase):
    def test_counts_and_versions(self):
        self.assertEqual(len(matrix("esp-idf")["include"]), 24)
        self.assertEqual(len(matrix("arduino")["include"]), 11)
        for framework, count in settings()["expected_counts"].items():
            self.assertEqual(len(discover(framework)), count)
        for entry in matrix("esp-idf")["include"]:
            self.assertEqual(entry["board"], "esp32_s3_lr1121_oled_1_54_xf")
            self.assertEqual(entry["bmgr_assist"], "0.8.3")
            self.assertEqual(entry["component_manager"], settings()["idf_component_manager"][entry["version"]])

    def test_every_name_and_path(self):
        for framework in ("esp-idf", "arduino"):
            examples = discover(framework)
            for e in examples:
                for selector in (e["name"], e["path"], "./" + e["path"], e["path"] + "/"):
                    self.assertEqual(select(examples, selector), [e])

    def test_idf62_covers_every_project_and_selector(self):
        entries = matrix("esp-idf", channel="development")["include"]
        self.assertEqual(len(entries), 12)
        self.assertEqual({e["path"] for e in entries}, {e["path"] for e in discover("esp-idf")})
        for e in entries:
            self.assertRegex(e["ref"], r"^[0-9a-f]{40}$")
            self.assertTrue(e["version"].startswith("v6.2-dev."))
            for selector in (e["name"], e["path"]):
                self.assertEqual(matrix("esp-idf", selector, channel="development")["include"], [e])
        with self.assertRaisesRegex(ValueError, "Unsupported framework/channel"):
            matrix("arduino", channel="development")

    def test_development_requires_fixed_revision_and_nonempty_matrix(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); self.fixture(base, "esp-idf")
            cfg = settings(); cfg["firmware_sources"] = []
            project = base / "examples/esp-idf/01_demo"
            (project / "main").mkdir(parents=True)
            (project / "CMakeLists.txt").write_text("")
            (project / "main/CMakeLists.txt").write_text("")
            for versions, message in (([], "empty matrix"),
                                      ([{"version": "v6.2-dev", "ref": "master"}], "full commit SHA"),
                                      ([{"version": "v6.2-dev.12345678", "ref": "a" * 40}], "does not match")):
                cfg["esp-idf-development"] = versions
                (base / "config/frameworks.json").write_text(json.dumps(cfg))
                with self.assertRaisesRegex(ValueError, message):
                    matrix("esp-idf", root=base, channel="development")

    def test_invalid_selectors_and_empty_matrix(self):
        for selector in ("", "unknown", "all ", "../01_i2c_scan", "/tmp", "*", "all,01_i2c_scan",
                         "libraries/RadioLib/examples", "firmware", "01_i2c_scan;echo"):
            with self.subTest(selector=selector), self.assertRaises(ValueError):
                select(discover("esp-idf"), selector)
        with self.assertRaisesRegex(ValueError, "empty matrix"):
            select([], "all")
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            select([{"name": "01_x", "path": "a"}, {"name": "01_x", "path": "b"}])

    def fixture(self, base, framework):
        (base / "config").mkdir()
        (base / "config/frameworks.json").write_text(json.dumps(settings()))
        (base / "examples" / framework).mkdir(parents=True)

    def test_nested_vendor_backup_and_generated_projects_are_excluded(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.fixture(base, "arduino")
            p = base / "examples/arduino/01_valid"
            p.mkdir(); (p / "01_valid.ino").write_text("")
            for child in ("libraries/RadioLib/examples/Foo", "examples/arduino/01_valid/backup/02_bad",
                          "examples/arduino/01_valid/build/03_bad"):
                q = base / child; q.mkdir(parents=True)
                (q / (q.name + ".ino")).write_text("")
            self.assertEqual([e["name"] for e in discover("arduino", base)], ["01_valid"])

    def test_invalid_directory_and_filename(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); self.fixture(base, "arduino")
            p = base / "examples/arduino/01_BAD"; p.mkdir()
            (p / "01_BAD.ino").write_text("")
            with self.assertRaisesRegex(ValueError, "Invalid example name"):
                discover("arduino", base)
            p.rename(p.with_name("01_valid"))
            with self.assertRaisesRegex(ValueError, "mismatch"):
                discover("arduino", base)

    def test_idf_generated_board_components_are_not_examples(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.fixture(base, "esp-idf")
            cfg = settings(); cfg["firmware_sources"] = []
            (base / "config/frameworks.json").write_text(json.dumps(cfg))
            project = base / "examples/esp-idf/01_valid"
            for directory in (project, project / "components/gen_bmgr_codes",
                              project / "managed_components/waveshare__waveshare_boards/ci/test_app"):
                (directory / "main").mkdir(parents=True)
                (directory / "CMakeLists.txt").write_text("include($ENV{IDF_PATH}/tools/cmake/project.cmake)\n")
                (directory / "main/CMakeLists.txt").write_text("idf_component_register()\n")
            self.assertEqual([e["name"] for e in discover("esp-idf", base)], ["01_valid"])

    def test_duplicate_firmware_name_and_empty_versions(self):
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); self.fixture(base, "esp-idf")
            cfg = settings(); cfg["firmware_sources"] = ["firmware/01_demo"]
            for entry in ("examples/esp-idf/01_demo", "firmware/01_demo"):
                p = base / entry; (p / "main").mkdir(parents=True)
                (p / "CMakeLists.txt").write_text("")
                (p / "main/CMakeLists.txt").write_text("")
            (base / "config/frameworks.json").write_text(json.dumps(cfg))
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                discover("esp-idf", base)
            cfg["firmware_sources"] = []; cfg["esp-idf"] = []
            (base / "config/frameworks.json").write_text(json.dumps(cfg))
            with self.assertRaisesRegex(ValueError, "empty matrix"):
                matrix("esp-idf", root=base)


if __name__ == "__main__":
    unittest.main()
