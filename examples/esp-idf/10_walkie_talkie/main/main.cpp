#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp_board_extra.h"
#include <stdint.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "codec2.h"
#include "lv_examples.h"
#include "ui.h"
#include "audio_stream_buffer.hpp"

static const char *TAG = "app_main";

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

volatile bool transmittedFlag = false;
SemaphoreHandle_t peripheral_mutex = NULL;

// set RF switch configuration for Wio WM1110
// Wio WM1110 uses DIO5 and DIO6 for RF switching
// NOTE: other boards may be different!
static const uint32_t rfswitch_dio_pins[] = { 
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  // mode                  DIO5  DIO6 
  { LR11x0::MODE_STBY,   { LOW,  LOW  } },
  { LR11x0::MODE_RX,     { HIGH, LOW  } },
  { LR11x0::MODE_TX,     { LOW,   HIGH } },
  { LR11x0::MODE_TX_HP,  { LOW,  HIGH } },
  { LR11x0::MODE_TX_HF,  { LOW,  LOW  } },
  { LR11x0::MODE_GNSS,   { LOW,  LOW  } },
  { LR11x0::MODE_WIFI,   { LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

void setFlag(void);

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
        hal->delay(10);
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

#define LORA_RX_BUFFER_SIZE (240 * 10)
#define AUDIO_BUFFER_SIZE (160 * 20)

DynamicBuffer<int16_t> iis_read_buffer(AUDIO_BUFFER_SIZE);
DynamicBuffer<int16_t> iis_write_buffer(AUDIO_BUFFER_SIZE);
DynamicBuffer<uint8_t> lora_send_buffer(LORA_RX_BUFFER_SIZE);
DynamicBuffer<uint8_t> lora_receive_buffer(LORA_RX_BUFFER_SIZE);

#define VOLUME_LEVEL 99
#define GAIN_LEVEL 30

uint8_t Send_Receive_Package[LORA_TRANSMISSION_DATA_SIZE];
static bool lastState = false;
static bool radio_tx_busy = false;

struct CODEC2 *codec2_state = NULL;
int codec2_sample_size = 0;
int codec2_compress_size = 0;
int16_t *codec2_buf = NULL;

void Codec2_Task(void *parameter);
void MAX_Play_Task(void *parameter);

static void hardware_init()
{
    peripheral_lock();
    bsp_bat_init(1000);
    bsp_get_bat_info(&bat_info);
    peripheral_unlock();
    
    peripheral_lock();
    bsp_rtc_init();
    bsp_get_rtc_time_date(&time_info);
    peripheral_unlock();
    
    bsp_sdcard_mount();

    peripheral_lock();
    int state = radio_apply_current_config(true);
    if (state != RADIOLIB_ERR_NONE) {
        peripheral_unlock();
        ESP_LOGE(TAG, "Radio init failed, state: %d", state);
        halt_task();
    }
    peripheral_unlock();
    ESP_LOGI(TAG, "Radio modem: %s", radio_modem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa");
    
    bsp_extra_codec_init();
    bsp_extra_codec_volume_set(VOLUME_LEVEL, nullptr);
    bsp_extra_codec_gain_set(GAIN_LEVEL, nullptr);
}

static void display_init()
{
    const bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = {
            .task_stack_size = (8 * 1024),
            .task_priority = ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY,
            .task_core_id = ESP_LV_ADAPTER_DEFAULT_TASK_CORE_ID,
            .tick_period_ms = ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS,
            .task_min_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MIN_DELAY_MS,
            .task_max_delay_ms = ESP_LV_ADAPTER_DEFAULT_TASK_MAX_DELAY_MS,
            .stack_in_psram = false,
        },
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT,
        .touch_flags = {0, 0, 0}
    };
    
    bsp_display_start_with_config(&cfg);
    bsp_display_lock(-1);
    ui_init();
    bsp_display_unlock();
    
    lv_timer_create(lvgl_rtc_bat_timer_cb, 300, NULL);
}

static void create_tasks()
{
    xTaskCreate(&Codec2_Task, "Codec2_Task", 32000, NULL, 6, NULL);
    xTaskCreate(&MAX_Play_Task, "MAX_Play_Task", 30000, NULL, 6, NULL);
}

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
        ESP_LOGE(TAG, "Radio reconfigure failed, state: %d", state);
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
        peripheral_lock();
        bsp_get_bat_info(&bat_info);
        bsp_get_rtc_time_date(&time_info);
        peripheral_unlock();
    }
}

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
        vTaskDelay(pdMS_TO_TICKS(10));
        
        if (Lora_Transmission_Mode) {
            for (uint8_t i = 0; i < msm_sample_frequency; i++) {
                size_t bytes_read = 0;
                bsp_extra_i2s_read(msm_sample_buf, pcm_frame_bytes, &bytes_read, portMAX_DELAY);
                
                if (bytes_read != pcm_frame_bytes) continue;
                
                codec2_encode(codec2_state, codec2_compress_buf, msm_sample_buf);
                lora_send_buffer.push(codec2_compress_buf, codec2_compress_size);
            }
        }
        
        while (true) {
            uint8_t lora_data_buf[codec2_compress_size];
            
            if (!lora_receive_buffer.pop(lora_data_buf, codec2_compress_size)) {
                break;
            }
            
            codec2_decode(codec2_state, codec2_buf, lora_data_buf);
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

void MAX_Play_Task(void *parameter)
{
    static constexpr size_t FRAME_SAMPLES = 160;
    static constexpr size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);
    int16_t iis_data_buf[FRAME_SAMPLES];
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        
        if (!iis_write_buffer.pop(iis_data_buf, FRAME_SAMPLES)) continue;
        
        size_t bytes_written = 0;
        bsp_extra_i2s_write(iis_data_buf, FRAME_BYTES, &bytes_written, portMAX_DELAY);
    }
}

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

extern "C" void app_main(void)
{
    peripheral_mutex = xSemaphoreCreateMutex();
    if (peripheral_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create peripheral mutex");
        halt_task();
    }

    hardware_init();
    display_init();
    create_tasks();
    
    uint32_t last_time = 0;
    
    while (1) {
        handle_radio_reconfigure();
        handle_ptt_key_change();
        handle_lora_transmission();
        
        if (transmittedFlag) {
            lora_irq_process();
        }
        
        handle_periodic_tasks(&last_time);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
