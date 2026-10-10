/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Wire.h"
#include "CodecDrivers.h"
#include "esp_rom_sys.h"

#include "Arduino.h"
#include "Audio.h"
#include "FS.h"
#include "SD_MMC.h"

#define PA_CTRL 8

// Button GPIOs
#define PIN_KEY_BOOT 0
#define PIN_KEY_PWR  4
#define PIN_KEY_MENU 5
#define PIN_KEY_PTT  6

// I2S GPIOs
#define I2S_SDOUT 16
#define I2S_BCLK 18
#define I2S_LRCK 21
#define I2S_MCLK 17

#define EXAMPLE_SAMPLE_RATE   (48000)
#define EXAMPLE_VOICE_VOLUME  (85)

#define MUSIC_DIR "/music"
#define MAX_SONGS 64
#define USER_VOLUME_MIN 0
#define USER_VOLUME_MAX 100
#define USER_VOLUME_DEFAULT 100
#define USER_VOLUME_STEP 5
#define AUDIO_VOLUME_STEPS 21

Audio audio;
Es8311Codec codec;

#define SD_MMC_CLK 12
#define SD_MMC_CMD 10
#define SD_MMC_D0  11

#define I2C_SDA 13
#define I2C_SCL 14

struct ButtonState {
  static constexpr uint32_t DEBOUNCE_MS = 25;
  static constexpr uint32_t REPEAT_DELAY_MS = 550;
  static constexpr uint32_t REPEAT_INTERVAL_MS = 180;

  uint8_t pin = 0;
  uint8_t activeLevel = LOW;
  bool stable = HIGH;
  bool lastRead = HIGH;
  bool clickEvent = false;
  bool repeatEvent = false;
  bool longActive = false;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;
  uint32_t lastRepeatAt = 0;

  void begin(uint8_t p, uint8_t pressedLevel = LOW) {
    pin = p;
    activeLevel = pressedLevel;
    pinMode(pin, activeLevel == LOW ? INPUT_PULLUP : INPUT);
    stable = digitalRead(pin);
    lastRead = stable;
    changedAt = millis();
  }

  void update() {
    const bool nowRead = digitalRead(pin);
    const uint32_t now = millis();

    if (nowRead != lastRead) {
      lastRead = nowRead;
      changedAt = now;
    }

    if ((now - changedAt) >= DEBOUNCE_MS && nowRead != stable) {
      stable = nowRead;
      if (stable == activeLevel) {
        pressedAt = now;
        lastRepeatAt = now;
        longActive = false;
      } else {
        if (pressedAt != 0 && !longActive) {
          clickEvent = true;
        }
        pressedAt = 0;
        lastRepeatAt = 0;
        longActive = false;
        repeatEvent = false;
      }
    }

    if (stable == activeLevel && pressedAt != 0 &&
        (now - pressedAt) >= REPEAT_DELAY_MS &&
        (now - lastRepeatAt) >= REPEAT_INTERVAL_MS) {
      repeatEvent = true;
      longActive = true;
      lastRepeatAt = now;
    }
  }

  bool takeClick() {
    const bool ret = clickEvent;
    clickEvent = false;
    return ret;
  }

  bool takeRepeat() {
    const bool ret = repeatEvent;
    repeatEvent = false;
    return ret;
  }
};

static ButtonState btnBoot;
static ButtonState btnPwr;
static ButtonState btnMenu;
static ButtonState btnPtt;

static String songs[MAX_SONGS];
static uint16_t songCount = 0;
static int16_t currentSong = -1;
static uint8_t userVolume = USER_VOLUME_DEFAULT;
static bool paused = false;
static volatile bool changingTrack = false;
static volatile bool eofPending = false;

static bool hasMp3Extension(const String &path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

static String musicPathFromEntryName(const char *entryName) {
  String path = entryName ? String(entryName) : String();
  path.replace("\\", "/");

  if (path.startsWith(String(MUSIC_DIR) + "/")) {
    return path;
  }
  if (path.startsWith("music/")) {
    return "/" + path;
  }

  const int slash = path.lastIndexOf('/');
  const String filename = slash >= 0 ? path.substring(slash + 1) : path;
  return String(MUSIC_DIR) + "/" + filename;
}

static void sortSongs() {
  for (uint16_t i = 0; i < songCount; i++) {
    for (uint16_t j = i + 1; j < songCount; j++) {
      if (songs[j].compareTo(songs[i]) < 0) {
        const String tmp = songs[i];
        songs[i] = songs[j];
        songs[j] = tmp;
      }
    }
  }
}

static void scanMusicFolder() {
  songCount = 0;

  File dir = SD_MMC.open(MUSIC_DIR);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("Music folder not found: %s\n", MUSIC_DIR);
    return;
  }

  while (songCount < MAX_SONGS) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    if (!entry.isDirectory()) {
      const String path = musicPathFromEntryName(entry.path() ? entry.path() : entry.name());
      if (hasMp3Extension(path)) {
        songs[songCount++] = path;
      }
    }

    entry.close();
  }
  dir.close();

  sortSongs();

  Serial.printf("Total MP3 songs in %s: %u\n", MUSIC_DIR, songCount);
  for (uint16_t i = 0; i < songCount; i++) {
    Serial.printf("  %u. %s\n", i + 1, songs[i].c_str());
  }
}

static uint8_t userVolumeToAudioVolume(uint8_t volume) {
  return (uint8_t)(((uint16_t)volume * AUDIO_VOLUME_STEPS + 50) / 100);
}

static void applyPlayerVolume() {
  const uint8_t audioVolume = userVolumeToAudioVolume(userVolume);
  audio.setVolume(audioVolume);
  Serial.printf("Volume: %u/100 (audio=%u/%u)\n", userVolume, audioVolume, AUDIO_VOLUME_STEPS);
}

static void setPlayerVolume(int volume) {
  if (volume < USER_VOLUME_MIN) {
    volume = USER_VOLUME_MIN;
  } else if (volume > USER_VOLUME_MAX) {
    volume = USER_VOLUME_MAX;
  }

  if (userVolume == (uint8_t)volume) {
    return;
  }

  userVolume = (uint8_t)volume;
  applyPlayerVolume();
}

static void volumeDown() {
  setPlayerVolume((int)userVolume - USER_VOLUME_STEP);
}

static void volumeUp() {
  setPlayerVolume((int)userVolume + USER_VOLUME_STEP);
}

static void playSong(int16_t index) {
  if (songCount == 0) {
    Serial.println("No MP3 songs to play");
    return;
  }

  while (index < 0) {
    index += songCount;
  }
  index %= songCount;

  changingTrack = true;
  eofPending = false;
  audio.stopSong();

  currentSong = index;
  paused = false;

  Serial.printf("Playing (%d/%u): %s\n", currentSong + 1, songCount, songs[currentSong].c_str());
  if (!audio.connecttoFS(SD_MMC, songs[currentSong].c_str())) {
    Serial.printf("connecttoFS failed: %s\n", songs[currentSong].c_str());
  }

  changingTrack = false;
}

static void nextSong() {
  playSong(currentSong < 0 ? 0 : currentSong + 1);
}

static void previousSong() {
  playSong(currentSong < 0 ? 0 : currentSong - 1);
}

static void togglePause() {
  if (songCount == 0 || currentSong < 0) {
    return;
  }

  audio.pauseResume();
  paused = !paused;
  Serial.println(paused ? "Paused" : "Resumed");
}

static void initButtons() {
  btnBoot.begin(PIN_KEY_BOOT);
  btnPwr.begin(PIN_KEY_PWR, HIGH);  // Q6 inverts the power key.
  btnMenu.begin(PIN_KEY_MENU);
  btnPtt.begin(PIN_KEY_PTT);

  Serial.println("Buttons:");
  Serial.println("  GPIO5 MENU: play / pause");
  Serial.println("  GPIO0 BOOT: previous");
  Serial.println("  GPIO4 PWR : next");
  Serial.println("  GPIO6 PTT : volume +, hold volume -");
}

static void handleButtons() {
  btnBoot.update();
  btnPwr.update();
  btnMenu.update();
  btnPtt.update();

  if (btnMenu.takeClick()) {
    togglePause();
  }
  if (btnBoot.takeClick()) {
    previousSong();
  }
  if (btnPwr.takeClick()) {
    nextSong();
  }
  if (btnPtt.takeClick()) {
    volumeUp();
  }

  if (btnPtt.takeRepeat()) {
    volumeDown();
  }
}

static void onAudioMessage(Audio::msg_t msg) {
  Serial.printf("[audio:%s] %s (%ld, %ld)\n",
                msg.s ? msg.s : "?",
                msg.msg ? msg.msg : "",
                (long)msg.arg1,
                (long)msg.arg2);

  if (msg.e == Audio::evt_eof && !changingTrack) {
    eofPending = true;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\r\nReset");
  Audio::audio_info_callback = onAudioMessage;

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!codec.begin(EXAMPLE_SAMPLE_RATE, EXAMPLE_VOICE_VOLUME)) {
    Serial.println("[ES8311] init failed");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("[ES8311] init OK, rate=%u, volume=%u\n", EXAMPLE_SAMPLE_RATE, EXAMPLE_VOICE_VOLUME);

  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdmmc", true, false, 20000)) {
    esp_rom_printf("Card Mount Failed\n");
    while (true) {
      delay(1000);
    }
  }

  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);
  Serial.printf("Power amp GPIO%d=%d\n", PA_CTRL, digitalRead(PA_CTRL));

  audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_MCLK);
  applyPlayerVolume();

  initButtons();
  scanMusicFolder();

  if (songCount > 0) {
    playSong(0);
  }
}

void loop() {
  handleButtons();
  audio.loop();

  if (eofPending) {
    eofPending = false;
    nextSong();
  }
}

void audio_info(const char *info) {
  Serial.print("info        ");
  Serial.println(info);
}

void audio_id3data(const char *info) {
  Serial.print("id3data     ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("eof_mp3     ");
  Serial.println(info);
  if (!changingTrack) {
    eofPending = true;
  }
}

void audio_showstation(const char *info) {
  Serial.print("station     ");
  Serial.println(info);
}

void audio_showstreaminfo(const char *info) {
  Serial.print("streaminfo  ");
  Serial.println(info);
}

void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle ");
  Serial.println(info);
}

void audio_bitrate(const char *info) {
  Serial.print("bitrate     ");
  Serial.println(info);
}

void audio_commercial(const char *info) {
  Serial.print("commercial  ");
  Serial.println(info);
}

void audio_icyurl(const char *info) {
  Serial.print("icyurl      ");
  Serial.println(info);
}

void audio_lasthost(const char *info) {
  Serial.print("lasthost    ");
  Serial.println(info);
}

void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  ");
  Serial.println(info);
}
