#include "Arduino.h"
#include "WiFiMulti.h"
#include "Audio.h"
#include "SD_MMC.h"
#include "FS.h"
#include "OneButton.h"

#include "es8311.h"
#include "esp_check.h"
#include "Wire.h"

#define PA_CTRL 8
#define I2S_MCLK 17
#define I2S_BCLK 18
#define I2S_DOUT 16
#define I2S_LRC 21

#define I2C_SDA 13
#define I2C_SCL 14

#define EXAMPLE_SAMPLE_RATE (16000)
#define EXAMPLE_MCLK_MULTIPLE (256)
#define EXAMPLE_MCLK_FREQ_HZ (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define EXAMPLE_VOICE_VOLUME (80)

Audio audio;
WiFiMulti wifiMulti;

#define GPIO0_IO_NUM  0
#define GPIO4_IO_NUM  4
#define GPIO5_IO_NUM  5
#define GPIO6_IO_NUM  6

#define BUTTON_ACTIVE_LEVEL true
#define GPIO4_ACTIVE_LEVEL false

OneButton button1(GPIO0_IO_NUM, BUTTON_ACTIVE_LEVEL);
OneButton button2(GPIO5_IO_NUM, BUTTON_ACTIVE_LEVEL);
OneButton button3(GPIO4_IO_NUM, GPIO4_ACTIVE_LEVEL);
OneButton button4(GPIO6_IO_NUM, BUTTON_ACTIVE_LEVEL);

#define MAX_SONGS 100
String songList[MAX_SONGS];
int songCount = 0;
int currentSong = 0;
bool isPaused = false;

int clk = 12;
int cmd = 10;
int d0 = 11;

static esp_err_t es8311_codec_init(void) {
  es8311_handle_t es_handle = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, "TAG", "es8311 create failed");
  const es8311_clock_config_t es_clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = EXAMPLE_MCLK_FREQ_HZ,
    .sample_frequency = EXAMPLE_SAMPLE_RATE
  };

  ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es_handle, EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE, EXAMPLE_SAMPLE_RATE), "TAG", "set es8311 sample frequency failed");
  ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL), "TAG", "set es8311 volume failed");
  return ESP_OK;
}

void scanSongs() {
  songCount = 0;
  File root = SD_MMC.open("/music");
  if (!root || !root.isDirectory()) {
    Serial.println("music folder not found");
    return;
  }

  File file = root.openNextFile();
  while (file && songCount < MAX_SONGS) {
    String name = file.name();
    if (name.endsWith(".mp3")) {
      songList[songCount++] = "music/" + name;
    }
    file = root.openNextFile();
  }
  Serial.print("Total songs: ");
  Serial.println(songCount);
}

void playSong(int index) {
  if (songCount == 0) return;

  index = (index % songCount + songCount) % songCount;
  currentSong = index;

  audio.stopSong();
  delay(50);
  audio.connecttoFS(SD_MMC, songList[currentSong].c_str());
  isPaused = false;

  Serial.print("Playing: ");
  Serial.println(songList[currentSong]);
}

void togglePause() {
  audio.pauseResume();
  isPaused = !isPaused;
  Serial.println(isPaused ? "Paused" : "Playing");
}

void volumeUp() {
  uint8_t vol = audio.getVolume();
  if (vol < 21) {
    audio.setVolume(vol + 1);
    Serial.print("Volume+: ");
    Serial.println(vol + 1);
  }
}

void volumeDown() {
  uint8_t vol = audio.getVolume();
  if (vol > 0) {
    audio.setVolume(vol - 1);
    Serial.print("Volume-: ");
    Serial.println(vol - 1);
  }
}

void click1() { playSong(currentSong - 1); }
void click2() { togglePause(); }
void click3() { playSong(currentSong + 1); }
void click4() { volumeUp(); }
void doubleclick4() { volumeDown(); }

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);
  es8311_codec_init();

  SD_MMC.setPins(clk, cmd, d0);
  SD_MMC.begin("/sdcard", true);

  scanSongs();

  button1.attachClick(click1);
  button2.attachClick(click2);
  button3.attachClick(click3);
  button4.attachClick(click4);
  button4.attachDoubleClick(doubleclick4);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(15);

  if (songCount > 0) playSong(0);
}

void loop() {
  audio.loop();

  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();

  if (!isPaused && songCount > 0 && !audio.isRunning()) {
    playSong(currentSong + 1);
  }

  delay(10);
}

void audio_info(const char *info) {}
void audio_id3data(const char *info) {}
void audio_eof_mp3(const char *info) {}
void audio_showstation(const char *info) {}
void audio_showstreamtitle(const char *info) {}
void audio_bitrate(const char *info) {}