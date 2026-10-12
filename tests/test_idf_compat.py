"""Exercise build-time compatibility fixes without an ESP-IDF installation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = "esp32_s3_lr1121_oled_1_54_xf"


@unittest.skipUnless(shutil.which("cmake"), "CMake is required")
class BoardCmakeTests(unittest.TestCase):
    def test_regeneration_is_idempotent_and_keeps_other_sources(self):
        for directory in ("components/esp32_s3_lr1121_oled_1_54_support/boards",
                          "managed_components/waveshare__waveshare_boards/boards"):
            with self.subTest(directory=directory):
                self.check_board_path(directory)

    def check_board_path(self, directory):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            board = root / directory / BOARD
            relative = ("../" if directory.startswith("components/") else "../../") + directory.removeprefix("components/") + "/" + BOARD
            board.mkdir(parents=True)
            for name in ("board_info.yaml", "board_devices.yaml", "board_peripherals.yaml"):
                (board / name).touch()
            script = root / "normalize.cmake"
            script.write_text(f'include("{ROOT}/config/esp_idf_board_compat.cmake")\n')
            command = ["cmake", "-P", str(script)]
            # Initial dependency resolution runs before bmgr creates any files.
            subprocess.run(command, cwd=root, check=True, capture_output=True)
            generated = root / "components/gen_bmgr_codes/CMakeLists.txt"
            generated.parent.mkdir(parents=True)
            (generated.parent / "board_manager.defaults").write_text(
                f'CONFIG_ESP_BOARD_NAME="{BOARD}"\n')
            original = (
                '# Generated board metadata\n'
                'idf_component_register(\n'
                f'  SRC_DIRS "." "{relative}" "extra"\n'
                f'  INCLUDE_DIRS "." "{relative}"\n'
                '  REQUIRES esp_board_manager)\n'
            )
            expected = original.replace(f' "{relative}"', '')
            for _ in range(2):
                # Simulate bmgr regenerating its original CMake file.
                generated.write_text(original)
                subprocess.run(command, cwd=root, check=True, capture_output=True)
                self.assertEqual(generated.read_text(), expected)
                timestamp = generated.stat().st_mtime_ns
                subprocess.run(command, cwd=root, check=True, capture_output=True)
                self.assertEqual(generated.stat().st_mtime_ns, timestamp)
            # Never silently exclude board code if the YAML-only contract changes.
            (board / "setup.c").touch()
            result = subprocess.run(command, cwd=root, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exactly the three product YAML files", ' '.join(result.stderr.split()))
            self.assertEqual(generated.read_text(), expected)

    def test_wrong_or_incomplete_board_fails_before_rewriting_generated_code(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            generated = root / "components/gen_bmgr_codes/CMakeLists.txt"
            generated.parent.mkdir(parents=True)
            original = '# A different product must never reach component registration.\n'
            generated.write_text(original)
            defaults = generated.parent / "board_manager.defaults"
            script = root / "check.cmake"
            script.write_text(f'include("{ROOT}/config/esp_idf_board_compat.cmake")\n')
            for content in (None, '', 'CONFIG_ESP_BOARD_NAME="esp_vocat_1_2"\n'):
                with self.subTest(defaults=content):
                    if content is not None:
                        defaults.write_text(content)
                    result = subprocess.run(["cmake", "-P", str(script)], cwd=root,
                                            capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    # Whitespace wrapping depends on the CMake version.
                    self.assertIn(f'idf.py bmgr -b {BOARD}', ' '.join(result.stderr.split()))
                    self.assertEqual(generated.read_text(), original)


# A minimal vendor target fixture with the pinned upstream name-copy statement.
# Tests verify source substitution and string boundaries, not ESP-IDF hardware.
MIXER_FIXTURE = r"""
#include "private_header.h"
#include <cstdlib>
#include <cstring>
struct stream_t { char name[16]; };
stream_t *new_stream(stream_t *cfg) {
    stream_t *stream = static_cast<stream_t *>(calloc(1, sizeof(stream_t)));
    if (cfg->name[0] != '\0') {
        strncpy(stream->name, cfg->name, sizeof(stream->name) - 1);
        stream->name[sizeof(stream->name) - 1] = 0;
    }
    return stream;
}
"""
NAME_TEST = r"""
#include <cassert>
#include <cstdlib>
#include <cstring>
struct stream_t { char name[16]; };
stream_t *new_stream(stream_t *);
int main() {
    for (unsigned n = 0; n <= 16; ++n) {
        stream_t input = {};
        memset(input.name, 'x', n);  // n=16 deliberately has no terminator.
        stream_t *output = new_stream(&input);
        unsigned len = n < 15 ? n : 15;
        assert(strlen(output->name) == len);
        assert(memcmp(output->name, input.name, len) == 0);
        for (unsigned i = len; i < 16; ++i) assert(output->name[i] == 0);
        free(output);
    }
}
"""


@unittest.skipUnless(shutil.which("cmake") and shutil.which("c++"), "CMake and C++ are required")
class AudioCmakeTests(unittest.TestCase):
    def test_build_copy_preserves_managed_source_and_name_boundaries(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            vendor = root / "vendor"
            vendor.mkdir()
            original = vendor / "audio_mixer.cpp"
            original.write_text(MIXER_FIXTURE)
            (vendor / "private_header.h").write_text("#pragma once\n")
            (vendor / "other.cpp").write_text("void untouched() {}\n")
            (vendor / "CMakeLists.txt").write_text(
                'add_library(player STATIC audio_mixer.cpp other.cpp)\n'
                'target_compile_options(player PRIVATE -O2 -Wall -Werror)\n')
            (root / "test.cpp").write_text(NAME_TEST)
            (root / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(audio_compat_test LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(vendor)
function(idf_component_get_property output component property)
    if(property STREQUAL "COMPONENT_LIB")
        set(${{output}} player PARENT_SCOPE)
    elseif(property STREQUAL "COMPONENT_DIR")
        set(${{output}} "${{CMAKE_CURRENT_SOURCE_DIR}}/vendor" PARENT_SCOPE)
    endif()
endfunction()
include("{ROOT}/config/esp_idf_audio_compat.cmake")
add_executable(name_test test.cpp)
target_link_libraries(name_test PRIVATE player)
''')
            configure = ["cmake", "-S", str(root), "-B", str(root / "build")]
            subprocess.run(configure, check=True, capture_output=True)
            subprocess.run(["cmake", "--build", str(root / "build")], check=True, capture_output=True)
            subprocess.run([str(root / "build/name_test")], check=True, capture_output=True)
            self.assertEqual(original.read_text(), MIXER_FIXTURE)
            patched = root / "build/ws_patched/esp-audio-player/audio_mixer.cpp"
            timestamp = patched.stat().st_mtime_ns
            subprocess.run(configure, check=True, capture_output=True)
            self.assertEqual(patched.stat().st_mtime_ns, timestamp)
            # A changed upstream implementation must trigger patch review.
            original.write_text(MIXER_FIXTURE.replace("strncpy(", "different_copy("))
            result = subprocess.run(configure, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("patch no longer matches", result.stderr)


@unittest.skipUnless(shutil.which("c++"), "C++ is required")
class RadioOverloadTests(unittest.TestCase):
    def test_real_headers_keep_default_and_explicit_overloads(self):
        source = r"""
#include <RadioLib.h>
void api(RF69& rf, LR1121& lr, SX1262& sx, SX1278& old, LR2021& newer,
         uint8_t *bytes, const uint8_t *constant,
         BellClient& bell, HellClient& hell, MorseClient& morse, RTTYClient& rtty) {
    rf.setOutputPower(10); rf.setOutputPower(10, true);
    rf.setSyncWord(bytes, 2); rf.setSyncWord(constant, 2);
    rf.setSyncWord(bytes, 2, 1); rf.setSyncWord(constant, 2, 1);
    lr.setOutputPower(10); lr.setOutputPower(10, false);
    lr.getTimeOnAir(240); newer.getTimeOnAir(240);
    sx.setOutputPower(10); old.getRSSI(); old.getRSSI(true);
    bell.write(uint8_t(1)); bell.write(bytes, 2);
    hell.write(uint8_t(1)); hell.write(bytes, 2);
    morse.write(uint8_t(1)); morse.write(bytes, 2);
    rtty.write(uint8_t(1)); rtty.write(bytes, 2);
}
"""
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "test.cpp"
            path.write_text(source)
            subprocess.run(["c++", "-std=c++17", "-Werror=overloaded-virtual", "-fsyntax-only",
                            "-I", str(ROOT / "libraries/RadioLib/src"), str(path)],
                           check=True, capture_output=True)
