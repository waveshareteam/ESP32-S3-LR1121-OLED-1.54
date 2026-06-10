#include <Arduino.h>
#include "driver/i2s_std.h"
#include <esp_vad.h>
#include "es7210.h"
#include "es8311.h"
#include "audio_stream_buffer.hpp"

#include <Wire.h>
#include <GaugeBQ27220.hpp>
#include "SensorPCF85063.hpp"
#include "ui/ui.h"

#include <lvgl.h>
#include <Arduino_GFX_Library.h>

#include <RadioLib.h>

#include "codec2.h"
// ==================== Pin Definitions ====================
#define PIN_I2S_BCLK          GPIO_NUM_18
#define PIN_I2S_LRCK          GPIO_NUM_21
#define PIN_I2S_MCLK          GPIO_NUM_15
#define PIN_ES8311_DOUT       GPIO_NUM_16
#define PIN_ES7210_DIN        GPIO_NUM_17
#define PIN_PA_CTRL           GPIO_NUM_8
#define PIN_I2C_SDA           GPIO_NUM_13
#define PIN_I2C_SCL           GPIO_NUM_14
#define PIN_SPI_MOSI          GPIO_NUM_45
#define PIN_SPI_MISO          GPIO_NUM_46
#define PIN_SPI_CLK           GPIO_NUM_40

#define EXAMPLE_LVGL_PALETTE_SIZE      8

// ==================== Audio ====================
#define SAMPLE_RATE           8000
#define FRAME_MS              30
#define FRAME_SAMPLES         (SAMPLE_RATE * FRAME_MS / 1000)
#define CHANNELS              1
#define BUFFER_SAMPLES        (FRAME_SAMPLES * CHANNELS)

#define MCLK_MULTIPLE         256
#define MCLK_FREQ             (SAMPLE_RATE * MCLK_MULTIPLE)

#define I2S_CH                I2S_NUM_1

#define VOLUME_LEVEL 80
#define GAIN_LEVEL GAIN_24DB

static i2s_chan_handle_t i2s_tx_chan;
static i2s_chan_handle_t i2s_rx_chan;

// ==================== ES8311 ====================
static esp_err_t es8311_codec_init(void)
{
  es8311_handle_t es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);

  const es8311_clock_config_t clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = MCLK_FREQ,
    .sample_frequency = SAMPLE_RATE
  };

  ESP_ERROR_CHECK(es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  es8311_sample_frequency_config(es, MCLK_FREQ, SAMPLE_RATE);
  es8311_voice_mute(es, false);
  es8311_voice_volume_set(es, VOLUME_LEVEL, NULL);
  es8311_microphone_gain_set(es, ES8311_MIC_GAIN_MIN);       // 关闭内部麦，避免干扰
    
  return ESP_OK;
}

// ==================== I2S Config ====================
#define I2S_GPIO_CFG { \
  .mclk = PIN_I2S_MCLK, \
  .bclk = PIN_I2S_BCLK, \
  .ws   = PIN_I2S_LRCK, \
  .dout = PIN_ES8311_DOUT, \
  .din  = PIN_ES7210_DIN, \
}

#define I2S_STD_CFG_STEREO(_rate) { \
  .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(_rate), \
  .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), \
  .gpio_cfg = I2S_GPIO_CFG, \
}



// ==================== ES7210 ====================
static esp_err_t es7210_codec_init(void)
{
    // ==================== ES7210 ADC ====================
    audio_hal_codec_config_t cfg = {
        .adc_input = AUDIO_HAL_ADC_INPUT_ALL,
        .codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE,
        .i2s_iface = {
            .mode = AUDIO_HAL_MODE_SLAVE,
            .fmt = AUDIO_HAL_I2S_NORMAL,
            .samples = AUDIO_HAL_08K_SAMPLES,
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
        },
    };

  es7210_adc_init(&Wire, &cfg);
  es7210_adc_config_i2s(cfg.codec_mode, &cfg.i2s_iface);
  es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2), GAIN_24DB);
  es7210_adc_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);

  return ESP_OK;
}

#define LORA_TRANSMISSION_DATA_SIZE 240
#define RADIO_FREQ_MHZ 868.0f
#define RADIO_BANDWIDTH_KHZ 125.0f
#define RADIO_SPREADING_FACTOR 7
#define RADIO_CODING_RATE 5
#define RADIO_SYNC_WORD 0x12
#define RADIO_OUTPUT_POWER_DBM 22
#define RADIO_PREAMBLE_LENGTH 8
#define RADIO_TCXO_VOLTAGE 3.0f
#define RADIO_GFSK_BIT_RATE_KBPS 50.0f
#define RADIO_GFSK_FREQ_DEV_KHZ 25.0f
#define RADIO_GFSK_RX_BW_KHZ 117.3f
#define RADIO_GFSK_PREAMBLE_LENGTH 32

/* ================== Arduino_GFX ================== */
Arduino_DataBus *bus = new Arduino_Wire(0x3C, 0x00, 0x40, &Wire);
Arduino_G *gfx = new Arduino_SSD1306(bus, GFX_NOT_DEFINED, 128, 64); //此库没有做ssd1309，用ssd1306也一样

/* ================== LVGL ================== */
lv_display_t *disp;
uint8_t *lvgl_buf;
uint8_t oled_buffer[128 * 64 / 8];

/* tick */
uint32_t millis_cb(void) {
  return millis();
}

// flag to indicate that a packet was sent or received
volatile bool transmittedFlag = false;
SemaphoreHandle_t peripheral_mutex = NULL;

static const uint32_t rfswitch_dio_pins[] = { 
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  // mode                  DIO5  DIO6 
  { LR1121::MODE_STBY,   { LOW,  LOW  } },
  { LR1121::MODE_RX,     { HIGH, LOW  } },
  { LR1121::MODE_TX,     { LOW,  HIGH } },
  { LR1121::MODE_TX_HP,  { LOW,  HIGH } },
  { LR1121::MODE_TX_HF,  { LOW,  LOW  } },
  { LR1121::MODE_GNSS,   { LOW,  LOW  } },
  { LR1121::MODE_WIFI,   { LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

/* ================== 关键：flush ================== */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    px_map += EXAMPLE_LVGL_PALETTE_SIZE;

    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            /* The order of bits is MSB first
                        MSB           LSB
               bits      7 6 5 4 3 2 1 0
               pixels    0 1 2 3 4 5 6 7
                        Left         Right
            */
            bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));

            /* Write to the buffer as required for the display.
            * It writes only 1-bit for monochrome displays mapped vertically.*/
            uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);
            if (chroma_color) {
                (*buf) &= ~(1 << (y % 8));
            } else {
                (*buf) |= (1 << (y % 8));
            }
        }
    }
    // pass the draw buffer to the driver

    gfx->drawBitmap(x1, y1, oled_buffer, x2 + 1, y2 + 1, 1, 0);

    lv_disp_flush_ready(disp);
}

void setFlag(void) {
  transmittedFlag = true;
}

static void halt_task()
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void halt_listen_error(int state)
{
    printf("[LR1121] Listen failed, code: %d\n", state);
    while (true) {
        delay(10);
    }
}

static void peripheral_lock()
{
    if (peripheral_mutex) {
        xSemaphoreTake(peripheral_mutex, portMAX_DELAY);
    }
}

static void peripheral_unlock()
{
    if (peripheral_mutex) {
        xSemaphoreGive(peripheral_mutex);
    }
}

static int radio_apply_current_config(bool start_receive)
{
    int state = RADIOLIB_ERR_NONE;

    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    if (radio_modem == RADIO_MODEM_GFSK) {
        state = radio.beginGFSK((float)radio_frequency_mhz,
                                RADIO_GFSK_BIT_RATE_KBPS,
                                RADIO_GFSK_FREQ_DEV_KHZ,
                                RADIO_GFSK_RX_BW_KHZ,
                                RADIO_OUTPUT_POWER_DBM,
                                RADIO_GFSK_PREAMBLE_LENGTH,
                                RADIO_TCXO_VOLTAGE);
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }

        uint8_t gfsk_sync_word[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
        state = radio.setSyncWord(gfsk_sync_word, sizeof(gfsk_sync_word));
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }
    } else {
        state = radio.begin((float)radio_frequency_mhz,
                            (float)radio_lora_bandwidth_khz,
                            (uint8_t)radio_lora_spreading_factor,
                            (uint8_t)radio_lora_coding_rate,
                            RADIO_SYNC_WORD,
                            RADIO_OUTPUT_POWER_DBM,
                            RADIO_PREAMBLE_LENGTH,
                            RADIO_TCXO_VOLTAGE);
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }

        state = radio.setCRC(0);
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }
    }

    radio.setIrqAction(setFlag);
    return start_receive ? radio.startReceive() : radio.standby();
}





// ==================== Constants ====================
#define LORA_RX_BUFFER_SIZE (240 * 10)  // 10 packets of data for LoRa receive
#define AUDIO_BUFFER_SIZE (160 * 20)     // 20 frames of audio data

// ==================== Data stream buffers ====================
// Plan One：Use circular buffer for low latency and fixed memory
// StaticBuffer<int16_t, AUDIO_BUFFER_SIZE> iis_read_buffer;      // Encode input
// StaticBuffer<int16_t, AUDIO_BUFFER_SIZE> iis_write_buffer;     // Decode output
// StaticBuffer<uint8_t, LORA_RX_BUFFER_SIZE> lora_send_buffer;   // LoRa send
// StaticBuffer<uint8_t, LORA_RX_BUFFER_SIZE> lora_receive_buffer; // LoRa receive
DynamicBuffer<int16_t> iis_read_buffer(AUDIO_BUFFER_SIZE);
DynamicBuffer<int16_t> iis_write_buffer(AUDIO_BUFFER_SIZE);
DynamicBuffer<uint8_t> lora_send_buffer(LORA_RX_BUFFER_SIZE);
DynamicBuffer<uint8_t> lora_receive_buffer(LORA_RX_BUFFER_SIZE);


uint8_t Send_Receive_Package[LORA_TRANSMISSION_DATA_SIZE];
static bool lastState = false;
static bool radio_tx_busy = false;

// ==================== Codec2 state ====================
struct CODEC2 *codec2_state = NULL;
int codec2_sample_size = 0;
int codec2_compress_size = 0;
int16_t *codec2_buf = NULL;


// ==================== Forward declarations of functions ====================
void Codec2_Task(void *parameter);
void MAX_Play_Task(void *parameter);

// ==================== Initialization functions ====================
static void hardware_init()
{
    peripheral_lock();
    //BQ27220
    if (!gauge.begin(Wire, PIN_I2C_SDA, PIN_I2C_SCL)) {
        Serial.println("Failed to BQ27220 - check your wiring!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("Init BQ27220 Sensor success!");
    gauge.setNewCapacity(1500, 1500);
    gauge.refresh();
    peripheral_unlock();

    peripheral_lock();
    //RTC
    if (!rtc.begin(Wire, PIN_I2C_SDA, PIN_I2C_SCL)) {
        Serial.println("Failed to find PCF85063 - check your wiring!");
        while (1) {
            delay(1000);
        }
    }
    Serial.println(rtc.strftime());
    peripheral_unlock();
    
    peripheral_lock();
    int state = radio_apply_current_config(true);
    if (state != RADIOLIB_ERR_NONE) {
        peripheral_unlock();
        printf("Radio init failed, state: %d\r\n", state);
        while (true) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    peripheral_unlock();

    //CODEC
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // ==================== I2S ====================
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_CH, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan);

    i2s_std_config_t std_cfg = I2S_STD_CFG_STEREO(SAMPLE_RATE);
    i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg);
    i2s_channel_enable(i2s_tx_chan);

    i2s_channel_init_std_mode(i2s_rx_chan, &std_cfg);
    i2s_channel_enable(i2s_rx_chan);

    // ==================== Initialize ES7210 (MIC1 + MIC2) ====================
    es7210_codec_init();

    es8311_codec_init();

    pinMode(PIN_PA_CTRL, OUTPUT);
    digitalWrite(PIN_PA_CTRL, 1);

    Serial.println("Init OK");
} 

static void display_init()
{
    gfx->begin();
    lv_init();
    lv_tick_set_cb(millis_cb);

    /* ⚠️ 1bit buffer（整屏） */
    uint32_t buf_size = 128 * 64 / 8 + EXAMPLE_LVGL_PALETTE_SIZE;
    lvgl_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);

    disp = lv_display_create(128, 64);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, lvgl_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ui_init();
    
    lv_timer_create(lvgl_rtc_bat_timer_cb, 300, NULL);
}

static void create_tasks()
{
    xTaskCreate(&Codec2_Task, "Codec2_Task", 32000, NULL, 6, NULL);
    xTaskCreate(&MAX_Play_Task, "MAX_Play_Task", 30000, NULL, 6, NULL);
}

// ==================== 主循环处理 ====================
static void handle_ptt_key_change()
{
    if (Boot_Key_Flag != lastState) {
        lastState = Boot_Key_Flag;
        
        if (Boot_Key_Flag) {
            Lora_Transmission_Mode = true;
            iis_read_buffer.clear();
            lora_send_buffer.clear();
        } else {
            Lora_Transmission_Mode = false;
            lora_send_buffer.clear();
            if (!radio_tx_busy) {
                peripheral_lock();
                int state = radio.startReceive();
                if (state != RADIOLIB_ERR_NONE) {
                    peripheral_unlock();
                    halt_listen_error(state);
                }
                peripheral_unlock();
            }
            iis_write_buffer.clear();
            lora_receive_buffer.clear();
        }
        
        lv_timer_t *t = lv_timer_create(lvgl_ptt_timer_cb, 1, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
}

static void handle_radio_reconfigure()
{
    if (!radio_reconfigure_requested || radio_tx_busy) {
        return;
    }

    radio_reconfigure_requested = false;
    transmittedFlag = false;
    iis_read_buffer.clear();
    iis_write_buffer.clear();
    lora_send_buffer.clear();
    lora_receive_buffer.clear();

    peripheral_lock();
    int state = radio_apply_current_config(!Boot_Key_Flag);
    peripheral_unlock();

    if (state != RADIOLIB_ERR_NONE) {
        printf("Radio reconfigure failed, state: %d\r\n", state);
        halt_task();
    }

    ESP_LOGI(TAG, "Radio modem: %s", radio_modem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa");
}

static void handle_lora_transmission()
{
    if (!Lora_Transmission_Mode) {
        return;
    }

    if (radio_tx_busy) {
        return;
    }

    if (!lora_send_buffer.pop(Send_Receive_Package, LORA_TRANSMISSION_DATA_SIZE)) {
        return;
    }
    
    printf("[LR1121] Sending packet...\r\n");

    peripheral_lock();
    int state = radio.standby();
    if (state == RADIOLIB_ERR_NONE) {
        state = radio.startTransmit(Send_Receive_Package, LORA_TRANSMISSION_DATA_SIZE);
    }
    transmittedFlag = false;
    if (state == RADIOLIB_ERR_NONE) {
        radio_tx_busy = true;
    } else {
        printf("[LR1121] Transmit failed, code: %d\n", state);
    }
    peripheral_unlock();
}

static void handle_periodic_tasks(uint32_t *last_time)
{
    uint32_t now = xTaskGetTickCount();
    if (now - *last_time >= pdMS_TO_TICKS(200)) {
        *last_time = now;
        gauge.refresh();
        // bsp_get_rtc_time_date(&time_info);
        Serial.println(rtc.strftime());
    }
}

// ==================== Codec2 任务 ====================
void Codec2_Task(void *parameter)
{
    const uint8_t msm_sample_frequency = LORA_TRANSMISSION_DATA_SIZE / 8;
    
    codec2_state = codec2_create(CODEC2_MODE_3200);
    if (!codec2_state) {
        printf("Failed to create Codec2\r\n");
        vTaskDelete(NULL);
        return;
    }
    
    codec2_set_lpc_post_filter(codec2_state, true, true, 0.5f, 0.7f);
    codec2_sample_size = codec2_samples_per_frame(codec2_state);
    codec2_compress_size = codec2_bytes_per_frame(codec2_state);
    
    const size_t pcm_frame_bytes = codec2_sample_size * sizeof(int16_t);

    int16_t *msm_sample_buf = (int16_t *)malloc(pcm_frame_bytes);
    codec2_buf = (int16_t *)malloc(pcm_frame_bytes);
    uint8_t *codec2_compress_buf = (uint8_t *)malloc(codec2_compress_size);
    
    if (!msm_sample_buf || !codec2_buf || !codec2_compress_buf) {
        printf("Codec2 buffer malloc failed\r\n");
        goto exit;
    }
    
    printf("Codec2 init OK | sample:%d compress:%d\n",
           codec2_sample_size, codec2_compress_size);
    
    while (1) {
        delay(10);
        
        // ===== Encode and send data =====
        if (Lora_Transmission_Mode) {
            for (uint8_t i = 0; i < msm_sample_frequency; i++) {
                size_t bytes_read = 0;
                i2s_channel_read(i2s_rx_chan, msm_sample_buf, pcm_frame_bytes, &bytes_read, portMAX_DELAY);
                
                if (bytes_read != pcm_frame_bytes) continue;
                
                codec2_encode(codec2_state, codec2_compress_buf, msm_sample_buf);
                lora_send_buffer.push(codec2_compress_buf, codec2_compress_size);
            }
        }
        
        // ===== Receive and decode data =====
        while (lora_receive_buffer.size() >= codec2_compress_size) {
            uint8_t lora_data_buf[codec2_compress_size];
            
            // Pop compressed data from the receive buffer
            memcpy(lora_data_buf, lora_receive_buffer.data(), codec2_compress_size);
            lora_receive_buffer.pop_front(codec2_compress_size);
            
            // Decode the compressed data
            codec2_decode(codec2_state, codec2_buf, lora_data_buf);
            
            // Add to the buffer for playback
            const size_t pcm_bytes = codec2_sample_size * sizeof(int16_t);
            iis_write_buffer.push(codec2_buf, codec2_sample_size);
        }
    }
    
exit:
    free(msm_sample_buf);
    free(codec2_buf);
    free(codec2_compress_buf);
    codec2_destroy(codec2_state);
    vTaskDelete(NULL);
}

// ==================== Audio playback task ====================
void MAX_Play_Task(void *parameter)
{
    static const size_t playback_frame_samples = 160;
    static const size_t FRAME_BYTES = playback_frame_samples * sizeof(int16_t);
    int16_t iis_data_buf[playback_frame_samples];
    
    while (1) {
        delay(10);
        
        if (iis_write_buffer.size() < playback_frame_samples) continue;
        
        // Pop a frame from the buffer for playback
        memcpy(iis_data_buf, iis_write_buffer.data(), FRAME_BYTES);
        iis_write_buffer.pop_front(playback_frame_samples);
        
        // Write to I2S
        size_t bytes_written = 0;
        i2s_channel_write(i2s_tx_chan, iis_data_buf, FRAME_BYTES, &bytes_written, portMAX_DELAY);
    }
}

// ==================== LoRa interrupt handler ====================
static void handle_rx_done()
{
    peripheral_lock();
    size_t raw_payload_len = radio.getPacketLength();
    size_t payload_len = raw_payload_len;
    if (payload_len > sizeof(Send_Receive_Package))
        payload_len = sizeof(Send_Receive_Package);

    if (payload_len == 0) {
        int state = radio.startReceive();
        if (state != RADIOLIB_ERR_NONE) {
            peripheral_unlock();
            halt_listen_error(state);
        }
        peripheral_unlock();
        return;
    }

    int read_state = radio.readData(Send_Receive_Package, payload_len);
    if (read_state == RADIOLIB_ERR_NONE && raw_payload_len == LORA_TRANSMISSION_DATA_SIZE) {
        printf("[LR1121] Packet received! Length: %u bytes\n", (unsigned)raw_payload_len);
        lora_receive_buffer.push(Send_Receive_Package, payload_len);
    } else {
        printf("[LR1121] Drop packet, len: %u, state: %d\n", (unsigned)raw_payload_len, read_state);
    }

    int state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        peripheral_unlock();
        halt_listen_error(state);
    }
    peripheral_unlock();
}

void lora_irq_process()
{
    transmittedFlag = false;

    if (radio_tx_busy) {
        peripheral_lock();
        int state = radio.finishTransmit();
        radio_tx_busy = false;
        if (state != RADIOLIB_ERR_NONE) {
            printf("[LR1121] Finish transmit failed, code: %d\n", state);
        }

        if (!Boot_Key_Flag) {
            state = radio.startReceive();
            if (state != RADIOLIB_ERR_NONE) {
                peripheral_unlock();
                halt_listen_error(state);
            }
        }
        peripheral_unlock();
        return;
    }

    if (Lora_Transmission_Mode) {
        return;
    }
    handle_rx_done();

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);
  peripheral_mutex = xSemaphoreCreateMutex();
  if (peripheral_mutex == NULL) {
        printf("Failed to create peripheral mutex\r\n");
        halt_task();
    }
  hardware_init();
  display_init();
  create_tasks();
  
}
uint32_t last_time = 0;
void loop() {

    lv_timer_handler();   // LVGL9
    // put your main code here, to run repeatedly:
    handle_radio_reconfigure();
    handle_ptt_key_change();
    handle_lora_transmission();
    
    if (transmittedFlag) {
        lora_irq_process();
    }
    
    handle_periodic_tasks(&last_time);

    delay(10);
    
}
