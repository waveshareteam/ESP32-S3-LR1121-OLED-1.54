"""Exercise the IDF 10 audio adapter with board YAML and mocked hardware I/O."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "examples/esp-idf/10_walkie_talkie"
sys.path.insert(0, str(ROOT / "scripts"))
from generate_board import board_directory

BOARD = board_directory("examples/esp-idf/10_walkie_talkie")

# Hardware calls are mocked; the implementation under test is the real product C file.
API = r"""
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define CONFIG_WS_BOARD_AUDIO 1
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_NOT_FOUND 3
#define ESP_ERR_INVALID_SIZE 4
#define ESP_ERR_TIMEOUT 5
#define ESP_RETURN_ON_FALSE(a, e, ...) do { if (!(a)) return (e); } while (0)
#define ESP_RETURN_ON_ERROR(a, ...) do { int err = (a); if (err) return err; } while (0)
#define ESP_LOGI(...) ((void)0)
#define CODEC_DEFAULT_BIT_WIDTH 16
#define CODEC_DEFAULT_ADC_VOLUME 24
#define CODEC_DEFAULT_CHANNEL 1
#define CODEC_DEFAULT_VOLUME 85
#define ESP_CODEC_DEV_MAKE_CHANNEL_MASK(n) (1U << (n))
typedef int esp_err_t;
typedef int i2s_slot_mode_t;
typedef void *i2s_chan_handle_t;
typedef void *esp_codec_dev_handle_t;
typedef struct { uint32_t sample_rate; int channel, bits_per_sample, channel_mask; } esp_codec_dev_sample_info_t;
typedef struct { int sample_rate; } dev_custom_audio_config_t;
#define I2S_DIR_TX 1
#define I2S_DIR_RX 2
#define I2S_COMM_MODE_STD 0
#define I2S_COMM_MODE_TDM 1
#define I2S_TDM_SLOT0 1
typedef struct {
    int direction, mode, pin_sentinel;
    union {
        struct { struct { int sample_rate_hz; } clk_cfg; struct { int total_slot, slot_mask; } slot_cfg; } std, tdm;
    } i2s_cfg;
} periph_i2s_config_t;
int esp_board_periph_init_custom(const char *, int (*)(void *, int, void **), int (*)(void *));
int esp_board_periph_deinit(const char *);
int periph_i2s_init(void *, int, void **);
int periph_i2s_deinit(void *);
typedef int AUDIO_PLAYER_MUTE_SETTING;
#define AUDIO_PLAYER_MUTE 1
#define AUDIO_PLAYER_UNMUTE 0
typedef struct { void *user_ctx; } audio_player_cb_ctx_t;
typedef void (*audio_player_cb_t)(audio_player_cb_ctx_t *);
typedef struct {
    esp_err_t (*mute_fn)(AUDIO_PLAYER_MUTE_SETTING);
    esp_err_t (*write_fn)(void *, size_t, size_t *, uint32_t);
    esp_err_t (*clk_set_fn)(uint32_t, uint32_t, i2s_slot_mode_t);
    int priority;
} audio_player_config_t;
typedef void file_iterator_instance_t;
int esp_board_manager_get_device_config(const char *, void **);
int esp_board_manager_get_periph_config(const char *, void **);
int board_peripheral(const char *, void **);
esp_codec_dev_handle_t board_audio_codec_speaker_init(uint32_t);
esp_codec_dev_handle_t board_audio_codec_microphone_init(uint32_t);
int esp_codec_dev_open(void *, esp_codec_dev_sample_info_t *);
int esp_codec_dev_close(void *);
int esp_codec_dev_set_in_gain(void *, int);
int esp_codec_dev_set_out_vol(void *, int);
int esp_codec_dev_set_out_mute(void *, bool);
int i2s_channel_read(void *, void *, size_t, size_t *, uint32_t);
int i2s_channel_write(void *, const void *, size_t, size_t *, uint32_t);
int board_extra_codec_mute_set(bool);
int audio_player_new(audio_player_config_t);
int audio_player_callback_register(audio_player_cb_t, void *);
int audio_player_delete(void);
int audio_player_play(FILE *);
file_iterator_instance_t *file_iterator_new(const char *);
int file_iterator_get_full_path_from_index(file_iterator_instance_t *, int, char *, size_t);
int file_iterator_get_index(file_iterator_instance_t *);
"""

HARNESS = r"""
#include "test_api.h"
#include "board_audio_player.c"
static int speaker, microphone, tx, rx;
static dev_custom_audio_config_t board_audio = { TEST_RATE };
static const periph_i2s_config_t board_rx = {
    .direction = I2S_DIR_RX, .mode = I2S_COMM_MODE_TDM, .pin_sentinel = 42,
    .i2s_cfg.tdm = { .clk_cfg = { TEST_RATE }, .slot_cfg = { TEST_SLOTS, 15 } }
};
static const periph_i2s_config_t board_tx = {
    .direction = I2S_DIR_TX, .mode = I2S_COMM_MODE_STD, .pin_sentinel = 42,
    .i2s_cfg.std.clk_cfg = { TEST_RATE }
};
static unsigned channel_inits;
static int channel_error;
int esp_board_periph_init_custom(const char *name, int (*init)(void *, int, void **), int (*deinit)(void *)) {
    const periph_i2s_config_t *config = !strcmp(name, "i2s_speaker") ? &board_tx : &board_rx;
    if (channel_error && config == &board_rx) return ESP_FAIL;
    void *handle = NULL;
    return init((void *)config, sizeof(*config), &handle);
}
int esp_board_periph_deinit(const char *name) { assert(!strcmp(name, "i2s_speaker")); return ESP_OK; }
int periph_i2s_init(void *data, int size, void **handle) {
    periph_i2s_config_t *cfg = data;
    assert(size == sizeof(*cfg) && cfg->pin_sentinel == 42);
    if (cfg->direction == I2S_DIR_RX) {
        assert(cfg->i2s_cfg.tdm.clk_cfg.sample_rate_hz == 8000);
        assert(cfg->i2s_cfg.tdm.slot_cfg.total_slot == 2);
        assert(cfg->i2s_cfg.tdm.slot_cfg.slot_mask == 1);
        *handle = &rx;
    } else {
        assert(cfg->i2s_cfg.std.clk_cfg.sample_rate_hz == 8000);
        *handle = &tx;
    }
    ++channel_inits;
    return ESP_OK;
}
int periph_i2s_deinit(void *handle) { return ESP_OK; }
static esp_codec_dev_sample_info_t speaker_format, microphone_format;
static unsigned opens;
static int open_error, io_error;
static size_t io_count = 320;
static uint32_t last_timeout;
int esp_board_manager_get_device_config(const char *name, void **out) {
    assert(!strcmp(name, "audio")); *out = &board_audio; return ESP_OK;
}
int esp_board_manager_get_periph_config(const char *name, void **out) {
    assert(!strcmp(name, "i2s_microphone")); *out = (void *)&board_rx; return ESP_OK;
}
int board_peripheral(const char *name, void **out) {
    assert(!strcmp(name, "i2s_speaker") || !strcmp(name, "i2s_microphone"));
    *out = !strcmp(name, "i2s_speaker") ? &tx : &rx; return ESP_OK;
}
void *board_audio_codec_speaker_init(uint32_t rate) { assert(rate == 8000); return &speaker; }
void *board_audio_codec_microphone_init(uint32_t rate) { assert(rate == 8000); return &microphone; }
int esp_codec_dev_open(void *dev, esp_codec_dev_sample_info_t *fs) {
    if (open_error) return open_error;
    assert(dev == &speaker || dev == &microphone);
    if (dev == &speaker) speaker_format = *fs; else microphone_format = *fs;
    ++opens; return ESP_OK;
}
int esp_codec_dev_close(void *dev) { assert(dev == &speaker || dev == &microphone); return ESP_OK; }
int esp_codec_dev_set_in_gain(void *dev, int gain) { assert(dev == &microphone); return ESP_OK; }
int esp_codec_dev_set_out_vol(void *dev, int vol) { assert(dev == &speaker); return ESP_OK; }
int esp_codec_dev_set_out_mute(void *dev, bool mute) { assert(dev == &speaker); return ESP_OK; }
int i2s_channel_read(void *handle, void *data, size_t len, size_t *count, uint32_t timeout) {
    assert(handle == &rx && len == 320); last_timeout = timeout;
    *count = io_count; memset(data, 0, io_count); return io_error;
}
int i2s_channel_write(void *handle, const void *data, size_t len, size_t *count, uint32_t timeout) {
    assert(handle == &tx && len == 320); last_timeout = timeout;
    *count = io_count; return io_error;
}
static void check_format(void) {
    assert(speaker_format.sample_rate == 8000 && microphone_format.sample_rate == 8000);
    assert(speaker_format.channel == 1 && speaker_format.bits_per_sample == 16);
    assert(microphone_format.channel == 2 && microphone_format.channel_mask == 1);
    assert(microphone_format.bits_per_sample == 16);
}
int main(int argc, char **argv) {
    assert(board_extra_codec_dev_resume() == ESP_ERR_INVALID_STATE);
    if (argc > 1 && !strcmp(argv[1], "channel-failure")) {
        channel_error = 1;
        assert(board_extra_codec_init() == ESP_FAIL && opens == 0);
        channel_error = 0;
    }
    if (argc > 1 && !strcmp(argv[1], "open-failure")) {
        open_error = ESP_FAIL;
        assert(board_extra_codec_init() == ESP_FAIL);
        assert(opens == 0);
        open_error = 0; // A failed open must not mark the adapter initialized.
    }
    assert(board_extra_codec_init() == ESP_OK && opens == 2);
    assert(board_extra_codec_sample_rate_get() == 8000);
    check_format();
    unsigned initialized = channel_inits;
    assert(board_extra_codec_init() == ESP_OK && channel_inits == initialized);
    assert(board_rx.i2s_cfg.tdm.clk_cfg.sample_rate_hz == TEST_RATE);
    assert(board_rx.i2s_cfg.tdm.slot_cfg.total_slot == TEST_SLOTS);
    assert(board_rx.i2s_cfg.tdm.slot_cfg.slot_mask == 15);
    int16_t pcm[160]; size_t count = 0;
    assert(board_extra_i2s_read(pcm, sizeof(pcm), &count, 100) == ESP_OK);
    assert(count == 320 && last_timeout == 100);
    assert(board_extra_i2s_write(pcm, sizeof(pcm), &count, 80) == ESP_OK);
    assert(count == 320 && last_timeout == 80);
    io_count = 80; io_error = ESP_ERR_TIMEOUT;
    assert(board_extra_i2s_read(pcm, sizeof(pcm), &count, 25) == ESP_ERR_TIMEOUT);
    assert(count == 80 && last_timeout == 25);
    assert(board_extra_i2s_write(pcm, sizeof(pcm), &count, 30) == ESP_ERR_TIMEOUT);
    assert(count == 80 && last_timeout == 30);
    assert(board_extra_codec_dev_stop() == ESP_OK);
    assert(board_extra_codec_dev_resume() == ESP_OK && opens == 4);
    check_format();
    return 0;
}
"""


class WalkieAudioTests(unittest.TestCase):
    def test_online_defaults_and_shared_clock(self):
        devices = {d["name"]: d for d in yaml.safe_load((BOARD / "board_devices.yaml").read_text())["devices"]}
        peripherals = {p["name"]: p for p in yaml.safe_load((BOARD / "board_peripherals.yaml").read_text())["peripherals"]}
        tx, rx = (peripherals[n]["config"] for n in ("i2s_speaker", "i2s_microphone"))
        self.assertEqual(devices["audio"]["config"]["sample_rate"], 16000)
        self.assertEqual(tx["sample_rate_hz"], 16000)
        self.assertEqual(rx["sample_rate_hz"], 16000)
        self.assertEqual((tx["data_bit_width"], rx["data_bit_width"]), (16, 16))
        self.assertEqual(rx["total_slot"], 4)
        self.assertEqual(rx["slot_mask"], "I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3")
        self.assertEqual(tx["slot_mode"], "I2S_SLOT_MODE_MONO")
        for pin in ("mclk", "bclk", "ws"):
            self.assertEqual(tx["pins"][pin], rx["pins"][pin])

    @unittest.skipUnless(shutil.which("cc"), "Host C compiler is required")
    def test_real_adapter_open_resume_timeout_and_error(self):
        devices = yaml.safe_load((BOARD / "board_devices.yaml").read_text())["devices"]
        rate = next(d["config"]["sample_rate"] for d in devices if d["name"] == "audio")
        peripherals = yaml.safe_load((BOARD / "board_peripherals.yaml").read_text())["peripherals"]
        slots = next(p["config"]["total_slot"] for p in peripherals if p["name"] == "i2s_microphone")
        with tempfile.TemporaryDirectory() as td:
            out = Path(td)
            (out / "test_api.h").write_text(API)
            for name in ("sdkconfig.h", "esp_log.h", "esp_check.h", "esp_codec_dev_defaults.h",
                         "esp_err.h", "esp_vfs_fat.h", "driver/i2s_std.h", "driver/gpio.h",
                         "board_extra.h", "gen_board_device_custom.h", "periph_i2s.h"):
                header = out / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "test_api.h"\n')
            (out / "test.c").write_text(HARNESS)
            subprocess.run(["cc", "-std=c11", "-Werror=implicit-function-declaration",
                            "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                            f"-DTEST_RATE={rate}", f"-DTEST_SLOTS={slots}",
                            "-I", str(out), "-I", str(PROJECT / "components/esp32_s3_lr1121_oled_1_54_support"),
                            str(out / "test.c"), "-o", str(out / "test")], check=True, capture_output=True)
            for args in ([], ["open-failure"], ["channel-failure"]):
                subprocess.run([str(out / "test"), *args], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
