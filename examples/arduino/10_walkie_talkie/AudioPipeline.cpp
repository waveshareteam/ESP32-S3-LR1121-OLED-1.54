/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AudioPipeline.h"

#include "AppState.h"
#include "CodecDrivers.h"
#include "Config.h"
#include "DynamicBuffer.h"

#include <string.h>

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
#include <codec2.h>
}

static Es8311Codec es8311;
static Es7210Adc es7210;
static i2s_chan_handle_t i2sTx = nullptr;
static i2s_chan_handle_t i2sRx = nullptr;
static DynamicBuffer<int16_t> playbackBuffer(AUDIO_BUFFER_SIZE);
static DynamicBuffer<uint8_t> loraSendBuffer(LORA_TRANSMISSION_DATA_SIZE * 10);
static DynamicBuffer<uint8_t> loraReceiveBuffer(LORA_TRANSMISSION_DATA_SIZE * 10);
static volatile uint32_t txEncodedFrames = 0;
static volatile uint32_t txMutedFrames = 0;
static volatile uint32_t micReadFailures = 0;
static volatile uint32_t micInputLevel = 0;
static volatile uint32_t micInputPeak = 0;
static volatile uint32_t micInputClipCount = 0;
static volatile uint32_t txProcessedLevel = 0;
static volatile uint32_t txProcessedPeak = 0;
static volatile uint32_t txProcessedClipCount = 0;
static volatile bool txVoiceGateOpen = false;
static volatile bool speakerPowerAmpEnabled = false;
static volatile int lastMicReadErr = ESP_OK;
static volatile uint32_t lastMicBytesRead = 0;
static volatile uint32_t lastMicBytesWanted = 0;
static volatile uint32_t rxDecodedFrames = 0;
static volatile uint32_t rxDecodedLevel = 0;
static volatile uint32_t rxDecodedPeak = 0;
static volatile uint32_t rxDecodedClipCount = 0;
static volatile uint32_t rxPlaybackLevel = 0;
static volatile uint32_t rxPlaybackPeak = 0;
static volatile uint32_t rxPlaybackClipCount = 0;
static uint8_t selectedMicSlot = 0;
static volatile bool audioSuspended = false;
static volatile bool codecTaskReady = false;
static volatile bool speakerTaskReady = false;
static volatile int startupError = 0;
static volatile uint32_t speakerFramesWritten = 0;
static volatile uint32_t speakerWriteFailures = 0;
static uint32_t lastAudioTxLog = 0;
static uint32_t lastAudioRxLog = 0;
static uint32_t lastMicFailureLog = 0;

static void setSpeakerPowerAmp(bool enable);

struct PcmStats {
  uint32_t avgAbs = 0;
  uint32_t peak = 0;
  uint32_t clipCount = 0;
};

static PcmStats measurePcm(const int16_t *samples, size_t sampleCount) {
  PcmStats stats;
  if (!samples || sampleCount == 0) {
    return stats;
  }

  uint32_t levelSum = 0;
  for (size_t i = 0; i < sampleCount; i++) {
    int32_t sample = samples[i];
    int32_t absSample = sample < 0 ? -sample : sample;
    levelSum += (uint32_t)absSample;
    if ((uint32_t)absSample > stats.peak) {
      stats.peak = (uint32_t)absSample;
    }
    if (absSample >= PCM_CLIP_LEVEL) {
      stats.clipCount++;
    }
  }
  stats.avgAbs = levelSum / sampleCount;
  return stats;
}

struct TxAudioProcessor {
  int16_t prevInput = 0;
  int32_t prevHighpass = 0;
  int32_t gainQ8 = 256;

  void reset() {
    prevInput = 0;
    prevHighpass = 0;
    gainQ8 = 256;
    txProcessedLevel = 0;
    txProcessedPeak = 0;
    txProcessedClipCount = 0;
    txVoiceGateOpen = false;
  }

  uint32_t highpassAndMeasure(int16_t *samples, size_t sampleCount) {
    uint32_t levelSum = 0;
    for (size_t i = 0; i < sampleCount; i++) {
      int32_t input = samples[i];
      int32_t highpass = input - prevInput + ((prevHighpass * TX_HIGHPASS_COEFF_Q15) >> 15);
      prevInput = (int16_t)input;
      prevHighpass = highpass;
      highpass = constrain(highpass, (int32_t)INT16_MIN, (int32_t)INT16_MAX);
      samples[i] = (int16_t)highpass;
      levelSum += highpass >= 0 ? highpass : -highpass;
    }
    return levelSum / sampleCount;
  }

  bool process(int16_t *samples, size_t sampleCount) {
    if (!samples || sampleCount == 0) {
      return false;
    }

    uint32_t level = highpassAndMeasure(samples, sampleCount);
    txVoiceGateOpen = level >= (uint32_t)TX_SILENCE_LEVEL;
    if (!txVoiceGateOpen) {
      memset(samples, 0, sampleCount * sizeof(int16_t));
      gainQ8 = 256;
      txProcessedLevel = 0;
      txProcessedPeak = 0;
      txProcessedClipCount = 0;
      txMutedFrames++;
      // Match IDF: encode silence too. Dropping these frames can prevent a
      // quiet talk burst from ever filling the 240-byte radio packet.
      return true;
    }

    int32_t targetGainQ8 = (TX_AGC_TARGET_LEVEL * 256) / (int32_t)level;
    targetGainQ8 = constrain(targetGainQ8, TX_AGC_MIN_GAIN_Q8, TX_AGC_MAX_GAIN_Q8);
    if (targetGainQ8 < gainQ8) {
      gainQ8 = (gainQ8 * 3 + targetGainQ8) / 4;
    } else {
      gainQ8 = (gainQ8 * 7 + targetGainQ8) / 8;
    }

    for (size_t i = 0; i < sampleCount; i++) {
      int32_t sample = ((int32_t)samples[i] * gainQ8) >> 8;
      samples[i] = (int16_t)constrain(sample, -TX_PEAK_LIMIT, TX_PEAK_LIMIT);
    }
    PcmStats stats = measurePcm(samples, sampleCount);
    txProcessedLevel = stats.avgAbs;
    txProcessedPeak = stats.peak;
    txProcessedClipCount = stats.clipCount;
    return true;
  }
};

struct PlaybackAudioProcessor {
  void process(int16_t *samples, size_t sampleCount) {
    if (!samples || sampleCount == 0) {
      return;
    }

    PcmStats decodedStats = measurePcm(samples, sampleCount);
    rxDecodedLevel = decodedStats.avgAbs;
    rxDecodedPeak = decodedStats.peak;
    rxDecodedClipCount = decodedStats.clipCount;

    // IDF scales and limits decoded PCM without a second noise gate. Preserve
    // quiet received speech instead of turning successfully received packets silent.
    for (size_t i = 0; i < sampleCount; i++) {
      int32_t sample = (int32_t)samples[i] * PLAYBACK_GAIN_NUM / PLAYBACK_GAIN_DEN;
      samples[i] = (int16_t)constrain(sample, -PLAYBACK_PEAK_LIMIT, PLAYBACK_PEAK_LIMIT);
    }

    PcmStats playbackStats = measurePcm(samples, sampleCount);
    rxPlaybackLevel = playbackStats.avgAbs;
    rxPlaybackPeak = playbackStats.peak;
    rxPlaybackClipCount = playbackStats.clipCount;
  }
};

static void logI2sInfo(const char *name, i2s_chan_handle_t channel) {
  if (!ENABLE_DEBUG_LOG) {
    return;
  }
  i2s_chan_info_t info = {};
  esp_err_t err = i2s_channel_get_info(channel, &info);
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[I2S] %s info failed: %d\n", name, err);
    return;
  }
  WALKIE_LOG_PRINTF("[I2S] %s mode=%d enabled=%d mclk=%u bclk=%u dma=%u\n",
                name,
                (int)info.mode,
                info.is_enabled ? 1 : 0,
                (unsigned)info.mclk_hz,
                (unsigned)info.bclk_hz,
                (unsigned)info.total_dma_buf_size);
}

static bool initI2s() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chanCfg.auto_clear = true;
  esp_err_t err = i2s_new_channel(&chanCfg, &i2sTx, &i2sRx);
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[I2S] new channel failed: %d\n", err);
    return false;
  }

  i2s_std_config_t stdCfg = {};
  stdCfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE);
  stdCfg.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  stdCfg.gpio_cfg.mclk = (gpio_num_t)PIN_I2S_MCLK;
  stdCfg.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  stdCfg.gpio_cfg.ws = (gpio_num_t)PIN_I2S_WS;
  stdCfg.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DOUT;
  stdCfg.gpio_cfg.din = (gpio_num_t)PIN_I2S_DIN;
  stdCfg.gpio_cfg.invert_flags.mclk_inv = false;
  stdCfg.gpio_cfg.invert_flags.bclk_inv = false;
  stdCfg.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_std_mode(i2sTx, &stdCfg);
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[I2S] TX init failed: %d\n", err);
    return false;
  }
  err = i2s_channel_enable(i2sTx);
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[I2S] TX enable failed: %d\n", err);
    return false;
  }

  i2s_tdm_config_t tdmCfg = {};
  tdmCfg.clk_cfg.sample_rate_hz = AUDIO_SAMPLE_RATE;
  tdmCfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
  tdmCfg.clk_cfg.ext_clk_freq_hz = 0;
  tdmCfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  tdmCfg.clk_cfg.bclk_div = 8;
  tdmCfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
  tdmCfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
  tdmCfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
  tdmCfg.slot_cfg.slot_mask = (i2s_tdm_slot_mask_t)I2S_TDM_SLOT0;
  tdmCfg.slot_cfg.ws_width = I2S_TDM_AUTO_WS_WIDTH;
  tdmCfg.slot_cfg.ws_pol = false;
  tdmCfg.slot_cfg.bit_shift = true;
  tdmCfg.slot_cfg.left_align = false;
  tdmCfg.slot_cfg.big_endian = false;
  tdmCfg.slot_cfg.bit_order_lsb = false;
  tdmCfg.slot_cfg.skip_mask = false;
  tdmCfg.slot_cfg.total_slot = AUDIO_MIC_TDM_TOTAL_SLOTS;
  tdmCfg.gpio_cfg.mclk = (gpio_num_t)PIN_I2S_MCLK;
  tdmCfg.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  tdmCfg.gpio_cfg.ws = (gpio_num_t)PIN_I2S_WS;
  tdmCfg.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DOUT;
  tdmCfg.gpio_cfg.din = (gpio_num_t)PIN_I2S_DIN;
  tdmCfg.gpio_cfg.invert_flags.mclk_inv = false;
  tdmCfg.gpio_cfg.invert_flags.bclk_inv = false;
  tdmCfg.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_tdm_mode(i2sRx, &tdmCfg);
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[I2S] RX init failed: %d\n", err);
    return false;
  }
  err = i2s_channel_enable(i2sRx);
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[I2S] RX enable failed: %d\n", err);
    return false;
  }
  logI2sInfo("TX", i2sTx);
  logI2sInfo("RX", i2sRx);
  return true;
}

bool initAudioHardware() {
  pinMode(PIN_POWER_AMP, OUTPUT);
  setSpeakerPowerAmp(false);
  if (!initI2s()) {
    return false;
  }
  bool ok = es8311.begin(AUDIO_SAMPLE_RATE, AUDIO_OUTPUT_VOLUME);
  ok &= es7210.begin();
  WALKIE_LOG_PRINTF("[Audio] codec init %s\n", ok ? "OK" : "failed");
  return ok;
}

static bool readMicFrame(int16_t *samples, size_t sampleCount) {
  size_t bytesRead = 0;
  size_t bytesToRead = sampleCount * sizeof(int16_t);
  esp_err_t err = i2s_channel_read(i2sRx, samples, bytesToRead, &bytesRead, AUDIO_MIC_READ_TIMEOUT_MS);
  if (err != ESP_OK || bytesRead != bytesToRead) {
    micReadFailures++;
    lastMicReadErr = err;
    lastMicBytesRead = (uint32_t)bytesRead;
    lastMicBytesWanted = (uint32_t)bytesToRead;
    micInputLevel = 0;
    micInputPeak = 0;
    micInputClipCount = 0;
    uint32_t now = millis();
    if (ENABLE_DEBUG_LOG && (micReadFailures <= 3 || now - lastMicFailureLog >= 1000)) {
      lastMicFailureLog = now;
      i2s_chan_info_t info = {};
      esp_err_t infoErr = i2s_channel_get_info(i2sRx, &info);
      WALKIE_LOG_PRINTF("[I2S] mic read failed err=%d bytes=%u want=%u timeout=%ums info=%d mode=%d enabled=%d mclk=%u bclk=%u dma=%u\n",
                    err,
                    (unsigned)bytesRead,
                    (unsigned)bytesToRead,
                    (unsigned)AUDIO_MIC_READ_TIMEOUT_MS,
                    infoErr,
                    infoErr == ESP_OK ? (int)info.mode : -1,
                    infoErr == ESP_OK && info.is_enabled ? 1 : 0,
                    infoErr == ESP_OK ? (unsigned)info.mclk_hz : 0,
                    infoErr == ESP_OK ? (unsigned)info.bclk_hz : 0,
                    infoErr == ESP_OK ? (unsigned)info.total_dma_buf_size : 0);
    }
    return false;
  }

  lastMicReadErr = ESP_OK;
  lastMicBytesRead = (uint32_t)bytesRead;
  lastMicBytesWanted = (uint32_t)bytesToRead;
  PcmStats stats = measurePcm(samples, sampleCount);
  micInputLevel = stats.avgAbs;
  micInputPeak = stats.peak;
  micInputClipCount = stats.clipCount;
  selectedMicSlot = 0;
  return true;
}

static void writeSpeakerFrame(const int16_t *samples, size_t sampleCount) {
  size_t bytesWritten = 0;
  size_t wanted = sampleCount * sizeof(int16_t);
  esp_err_t err = i2s_channel_write(i2sTx, samples, wanted, &bytesWritten, AUDIO_MIC_READ_TIMEOUT_MS);
  if (err == ESP_OK && bytesWritten == wanted) {
    speakerFramesWritten++;
  } else {
    speakerWriteFailures++;
  }
}

static void setSpeakerPowerAmp(bool enable) {
  if (speakerPowerAmpEnabled == enable) {
    return;
  }
  digitalWrite(PIN_POWER_AMP, enable ? HIGH : LOW);
  speakerPowerAmpEnabled = enable;
}

void clearAudioBuffers() {
  playbackBuffer.clear();
  loraSendBuffer.clear();
  loraReceiveBuffer.clear();
}

static void logAudioSleepStep(const char *step, esp_err_t err) {
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    WALKIE_LOG_PRINTF("[Audio] %s failed: %s\n", step, esp_err_to_name(err));
  }
}

void prepareAudioSleep() {
  audioSuspended = true;
  delay(AUDIO_MIC_READ_TIMEOUT_MS + 20);
  clearAudioBuffers();
  setSpeakerPowerAmp(false);
  if (i2sTx) {
    logAudioSleepStep("TX disable", i2s_channel_disable(i2sTx));
  }
  if (i2sRx) {
    logAudioSleepStep("RX disable", i2s_channel_disable(i2sRx));
  }
  if (!es8311.sleep()) {
    WALKIE_LOG_PRINTLN("[Audio] ES8311 sleep failed");
  }
  if (!es7210.sleep()) {
    WALKIE_LOG_PRINTLN("[Audio] ES7210 sleep failed");
  }
}

void resumeAudioFromSleep() {
  if (!es8311.resume(AUDIO_SAMPLE_RATE, AUDIO_OUTPUT_VOLUME)) {
    WALKIE_LOG_PRINTLN("[Audio] ES8311 resume failed");
  }
  if (!es7210.resume()) {
    WALKIE_LOG_PRINTLN("[Audio] ES7210 resume failed");
  }
  if (i2sTx) {
    logAudioSleepStep("TX enable", i2s_channel_enable(i2sTx));
  }
  if (i2sRx) {
    logAudioSleepStep("RX enable", i2s_channel_enable(i2sRx));
  }
  clearAudioBuffers();
  audioSuspended = false;
}

static void codec2Task(void *parameter) {
  (void)parameter;
  struct CODEC2 *codec = codec2_create(CODEC2_MODE_3200);
  if (!codec) {
    startupError = 1;
    WALKIE_LOG_PRINTLN("[Codec2] create failed");
    vTaskDelete(nullptr);
    return;
  }

  codec2_set_lpc_post_filter(codec, true, true, 0.5f, 0.7f);
  int sampleCount = codec2_samples_per_frame(codec);
  int compressSize = codec2_bytes_per_frame(codec);
  uint8_t framesPerPacket = LORA_TRANSMISSION_DATA_SIZE / compressSize;

  int16_t *micSamples = (int16_t *)malloc(sampleCount * sizeof(int16_t));
  int16_t *decodedSamples = (int16_t *)malloc(sampleCount * sizeof(int16_t));
  uint8_t *bits = (uint8_t *)malloc(compressSize);
  if (!micSamples || !decodedSamples || !bits) {
    startupError = 2;
    WALKIE_LOG_PRINTLN("[Codec2] buffer allocation failed");
    free(micSamples);
    free(decodedSamples);
    free(bits);
    codec2_destroy(codec);
    vTaskDelete(nullptr);
    return;
  }

  WALKIE_LOG_PRINTF("[Codec2] samples=%d bytes=%d packet_frames=%u\n", sampleCount, compressSize, framesPerPacket);
  codecTaskReady = true;

  bool txAudioActive = false;
  uint8_t txSkipFrames = 0;
  TxAudioProcessor txProcessor;
  PlaybackAudioProcessor playbackProcessor;

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10));

    if (audioSuspended) {
      txAudioActive = false;
      txProcessor.reset();
      continue;
    }

    if (transmissionMode) {
      if (!txAudioActive) {
        txAudioActive = true;
        txSkipFrames = TX_START_SKIP_FRAMES;
        txProcessor.reset();
        lastAudioTxLog = millis();
        WALKIE_LOG_PRINTF("[Audio] TX start, mic slot=%u\n", selectedMicSlot);
      }

      for (uint8_t i = 0; i < framesPerPacket; i++) {
        // A packet contains 30 frames. Yield and recheck PTT on every frame,
        // including muted frames and while draining queued I2S samples.
        vTaskDelay(1);
        if (!transmissionMode || audioSuspended) {
          break;
        }
        bool micReadOk = readMicFrame(micSamples, sampleCount);
        if (!transmissionMode || audioSuspended) {
          break;
        }
        // An I2S failure is not a valid silent microphone frame.
        if (!micReadOk) {
          continue;
        }
        if (txSkipFrames > 0) {
          txSkipFrames--;
          continue;
        }
        if (!txProcessor.process(micSamples, sampleCount)) {
          continue;
        }
        codec2_encode(codec, bits, (short *)micSamples);
        // Slow LoRa settings cannot carry continuous Codec2 audio. Bound the
        // backlog to the preallocated ten packets instead of growing forever.
        if (transmissionMode && !audioSuspended &&
            loraSendBuffer.available() + compressSize <= LORA_TRANSMISSION_DATA_SIZE * 10) {
          loraSendBuffer.push(bits, compressSize);
        }
        txEncodedFrames++;
      }

      uint32_t now = millis();
      if (now - lastAudioTxLog >= 1000) {
        lastAudioTxLog = now;
        WALKIE_LOG_PRINTF("[Audio] tx queued=%uB frames=%u muted=%u mic_fail=%u slot=%u raw=%u/%u clip=%u proc=%u/%u clip=%u threshold=%u voice=%u last_err=%d bytes=%u/%u\n",
                      (unsigned)loraSendBuffer.available(),
                      (unsigned)txEncodedFrames,
                      (unsigned)txMutedFrames,
                      (unsigned)micReadFailures,
                      selectedMicSlot,
                      (unsigned)micInputLevel,
                      (unsigned)micInputPeak,
                      (unsigned)micInputClipCount,
                      (unsigned)txProcessedLevel,
                      (unsigned)txProcessedPeak,
                      (unsigned)txProcessedClipCount,
                      (unsigned)TX_SILENCE_LEVEL,
                      txVoiceGateOpen ? 1 : 0,
                      (int)lastMicReadErr,
                      (unsigned)lastMicBytesRead,
                      (unsigned)lastMicBytesWanted);
      }
    } else {
      if (txAudioActive) {
        WALKIE_LOG_PRINTF("[Audio] TX stop, queued=%uB frames=%u muted=%u mic_fail=%u slot=%u raw=%u/%u clip=%u proc=%u/%u clip=%u threshold=%u voice=%u last_err=%d bytes=%u/%u\n",
                      (unsigned)loraSendBuffer.available(),
                      (unsigned)txEncodedFrames,
                      (unsigned)txMutedFrames,
                      (unsigned)micReadFailures,
                      selectedMicSlot,
                      (unsigned)micInputLevel,
                      (unsigned)micInputPeak,
                      (unsigned)micInputClipCount,
                      (unsigned)txProcessedLevel,
                      (unsigned)txProcessedPeak,
                      (unsigned)txProcessedClipCount,
                      (unsigned)TX_SILENCE_LEVEL,
                      txVoiceGateOpen ? 1 : 0,
                      (int)lastMicReadErr,
                      (unsigned)lastMicBytesRead,
                      (unsigned)lastMicBytesWanted);
      }
      txAudioActive = false;
      txProcessor.reset();
    }

    while (!transmissionMode && !audioSuspended && loraReceiveBuffer.pop(bits, compressSize)) {
      codec2_decode(codec, (short *)decodedSamples, bits);
      if (transmissionMode || audioSuspended) {
        break;
      }
      playbackProcessor.process(decodedSamples, sampleCount);
      playbackBuffer.push(decodedSamples, sampleCount);
      rxDecodedFrames++;
      vTaskDelay(1);

      uint32_t now = millis();
      if (now - lastAudioRxLog >= 1000) {
        lastAudioRxLog = now;
        WALKIE_LOG_PRINTF("[Audio] rx frames=%u rfq=%uB playq=%uS dec=%u/%u clip=%u out=%u/%u clip=%u pa=%u\n",
                      (unsigned)rxDecodedFrames,
                      (unsigned)loraReceiveBuffer.available(),
                      (unsigned)playbackBuffer.available(),
                      (unsigned)rxDecodedLevel,
                      (unsigned)rxDecodedPeak,
                      (unsigned)rxDecodedClipCount,
                      (unsigned)rxPlaybackLevel,
                      (unsigned)rxPlaybackPeak,
                      (unsigned)rxPlaybackClipCount,
                      speakerPowerAmpEnabled ? 1 : 0);
      }
    }
  }
}

static void speakerTask(void *parameter) {
  (void)parameter;
  speakerTaskReady = true;
  static constexpr size_t frameSamples = 160;
  int16_t out[frameSamples];
  uint32_t lastAudioFrameAt = 0;
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10));
    if (audioSuspended) {
      setSpeakerPowerAmp(false);
      lastAudioFrameAt = millis();
      continue;
    }
    bool hasAudio = playbackBuffer.pop(out, frameSamples);
    if (hasAudio) {
      lastAudioFrameAt = millis();
      setSpeakerPowerAmp(true);
    } else {
      memset(out, 0, sizeof(out));
      if (speakerPowerAmpEnabled && millis() - lastAudioFrameAt >= AUDIO_SPEAKER_IDLE_MUTE_MS) {
        setSpeakerPowerAmp(false);
      }
    }
    writeSpeakerFrame(out, frameSamples);
  }
}

bool startAudioTasks() {
  if (xTaskCreate(codec2Task, "Codec2_Task", 32000, nullptr, 6, nullptr) != pdPASS) {
    startupError = 3;
    return false;
  }
  if (xTaskCreate(speakerTask, "Speaker_Task", 30000, nullptr, 6, nullptr) != pdPASS) {
    startupError = 4;
    return false;
  }
  return true;
}

int audioStartupError() {
  return startupError;
}

void logAudioDiagnostics() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (!VOICE_DEBUG_LOG || now - lastLog < 2000) {
    return;
  }
  lastLog = now;
  WALKIE_LOG_PRINTF("[AudioStatus] codec=%u speaker=%u error=%d tx_frames=%u rx_frames=%u "
                "txq=%uB mic_fail=%u play_frames=%u play_fail=%u pa=%u heap=%u psram=%u\n",
                unsigned(codecTaskReady), unsigned(speakerTaskReady), int(startupError),
                unsigned(txEncodedFrames), unsigned(rxDecodedFrames), unsigned(loraSendBuffer.available()),
                unsigned(micReadFailures), unsigned(speakerFramesWritten), unsigned(speakerWriteFailures),
                unsigned(speakerPowerAmpEnabled), unsigned(ESP.getFreeHeap()), unsigned(ESP.getFreePsram()));
}

bool audioPopTxPacket(uint8_t *packet, size_t packetSize) {
  if (!packet || packetSize != LORA_TRANSMISSION_DATA_SIZE) {
    return false;
  }
  return loraSendBuffer.pop(packet, LORA_TRANSMISSION_DATA_SIZE);
}

void audioPushRxPacket(const uint8_t *packet, size_t packetSize) {
  if (!packet || packetSize != LORA_TRANSMISSION_DATA_SIZE) {
    return;
  }
  loraReceiveBuffer.push(packet, LORA_TRANSMISSION_DATA_SIZE);
}

size_t audioTxQueuedBytes() {
  return loraSendBuffer.available();
}

uint32_t audioTxEncodedFrames() {
  return txEncodedFrames;
}

uint32_t audioMicReadFailures() {
  return micReadFailures;
}

uint8_t audioSelectedMicSlot() {
  return selectedMicSlot;
}
