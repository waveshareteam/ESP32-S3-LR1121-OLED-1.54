"""Check downloaded board integrity and latest stable selection without network I/O."""
import hashlib
import io
from pathlib import Path
import sys
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from fetch_board import YAML_FILES, latest_release, verified_profile


class BoardProfileTests(unittest.TestCase):
    def fixture(self, names=None):
        content = io.BytesIO()
        files = []
        with zipfile.ZipFile(content, "w") as archive:
            for name in names or [f"boards/product/{n}" for n in sorted(YAML_FILES)]:
                data = b"board: product\n"
                archive.writestr(name, data)
                files.append({"path": name, "size": len(data), "hash": hashlib.sha256(data).hexdigest()})
        return content.getvalue(), {"algorithm": "sha256", "files": files}

    def test_verified_product_and_corruption(self):
        archive, checks = self.fixture()
        self.assertEqual(set(verified_profile(archive, checks, "product")), YAML_FILES)
        checks["files"][0]["hash"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            verified_profile(archive, checks, "product")

    def test_missing_extra_and_unsafe_files_fail(self):
        for names in (["boards/product/board_info.yaml"],
                      [f"boards/product/{n}" for n in YAML_FILES] + ["boards/product/driver.c"],
                      ["../outside"]):
            with self.subTest(names=names):
                with self.assertRaises(ValueError):
                    verified_profile(*self.fixture(names), "product")

    def test_latest_stable_excludes_yanked_and_preview(self):
        metadata = {"versions": [{"version": "0.1.2"}, {"version": "0.1.11"},
                                 {"version": "2.0.0-rc1"}, {"version": "2.0.0", "yanked_at": "date"}]}
        self.assertEqual(latest_release(metadata)["version"], "0.1.11")
        with self.assertRaisesRegex(ValueError, "No stable"):
            latest_release({"versions": []})
