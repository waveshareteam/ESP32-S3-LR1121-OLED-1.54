"""Verify saved Arduino frequencies survive the IDF menu/NVS conversion."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "examples/esp-idf/10_walkie_talkie/main/app"

NVS = r"""
#pragma once
#include <stdint.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
#define ESP_OK 0
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char *, int, nvs_handle_t *);
int nvs_get_u8(nvs_handle_t, const char *, uint8_t *);
int nvs_get_u16(nvs_handle_t, const char *, uint16_t *);
int nvs_set_u8(nvs_handle_t, const char *, uint8_t);
int nvs_set_u16(nvs_handle_t, const char *, uint16_t);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
"""

HARNESS = r"""
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include "radio_settings.h"
#include "nvs.h"
static uint16_t stored_freq;
static uint8_t stored_band;
int nvs_open(const char *name, int, nvs_handle_t *h) {
    assert(!strcmp(name, "radio")); *h = 1; return ESP_OK;
}
int nvs_get_u8(nvs_handle_t, const char *key, uint8_t *v) {
    if (!strcmp(key, "band")) { *v = stored_band; return ESP_OK; } return -1;
}
int nvs_get_u16(nvs_handle_t, const char *key, uint16_t *v) {
    assert(!strcmp(key, "freq")); *v = stored_freq; return ESP_OK;
}
int nvs_set_u8(nvs_handle_t, const char *key, uint8_t v) {
    if (!strcmp(key, "band")) stored_band = v; return ESP_OK;
}
int nvs_set_u16(nvs_handle_t, const char *key, uint16_t v) {
    assert(!strcmp(key, "freq")); stored_freq = v; return ESP_OK;
}
int nvs_commit(nvs_handle_t) { return ESP_OK; }
void nvs_close(nvs_handle_t) {}
int main() {
    for (radio_band_t band : {RADIO_BAND_HF, RADIO_BAND_LF}) {
        int low = band == RADIO_BAND_HF ? 862 : 410;
        int high = band == RADIO_BAND_HF ? 923 : 510;
        stored_band = band;
        radio_select_band(band);
        std::istringstream options(radio_frequency_options());
        for (int f = low; f <= high; ++f) {
            std::string option;
            assert(std::getline(options, option) && std::stoi(option) == f);
            stored_freq = f; // Same namespace and uint16 frequency used by Arduino.
            radio_settings_load();
            assert(radio_frequency_mhz == f && last_freq_mhz == f);
            auto index = radio_freq_to_index(f);
            assert(index == f - low && radio_index_to_freq(index) == f);
            radio_settings_save();
            assert(stored_freq == f && stored_band == band);
        }
        std::string extra; assert(!std::getline(options, extra));
        assert(radio_index_to_freq(255) == high);
        assert(radio_freq_to_index(low - 10) == 0);
        stored_freq = 0;
        radio_settings_load();
        assert(radio_frequency_mhz == (band == RADIO_BAND_HF ? 868 : 433));
    }
}
"""


class WalkieRadioTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "Host C++ compiler is required")
    def test_nvs_frequency_menu_round_trip(self):
        with tempfile.TemporaryDirectory() as td:
            out = Path(td)
            (out / "nvs.h").write_text(NVS)
            (out / "board_support.h").write_text("#pragma once\ntypedef int pcf85063a_datetime_t;\ntypedef int bsp_bat_info_t;\n")
            (out / "test.cpp").write_text(HARNESS)
            subprocess.run(["c++", "-std=c++17", "-I", str(out), "-I", str(APP),
                            str(APP / "radio_settings.cpp"), str(APP / "app_state.cpp"),
                            str(out / "test.cpp"), "-o", str(out / "test")], check=True, capture_output=True)
            subprocess.run([str(out / "test")], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
