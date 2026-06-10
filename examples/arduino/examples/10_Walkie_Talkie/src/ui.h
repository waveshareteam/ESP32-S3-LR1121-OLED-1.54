#ifndef __UI_H__
#define __UI_H__

#include <RadioLib.h>
#include <SPI.h>
#include "lvgl.h"
 

#ifdef __cplusplus
extern "C" {
#endif

#define LR1121_HF 1

#define PAYLOAD_LENGTH 7

typedef enum {
    RADIO_MODEM_GFSK = 0,
    RADIO_MODEM_LORA = 1,
} radio_modem_t;

extern Module module;
extern LR1121 radio;
extern SemaphoreHandle_t peripheral_mutex;
extern volatile radio_modem_t radio_modem;
extern volatile bool radio_reconfigure_requested;
extern volatile float radio_frequency_mhz;
extern volatile uint8_t radio_lora_spreading_factor;
extern volatile float radio_lora_bandwidth_khz;
extern volatile uint8_t radio_lora_coding_rate;

extern bool Boot_Key_Flag;
extern bool Lora_Transmission_Mode; // 默认为接收模式
extern bool Lora_Set_Mode; 

extern GaugeBQ27220 gauge;
extern SensorPCF85063 rtc;

LV_IMAGE_DECLARE(Battery4_png);    // png/Battery4.png
LV_IMAGE_DECLARE(Battery3_png);    // png/Battery3.png
LV_IMAGE_DECLARE(Battery2_png);    // png/Battery2.png
LV_IMAGE_DECLARE(Battery1_png);    // png/Battery1.png
LV_IMAGE_DECLARE(Charge_png);    // png/Charge.png
LV_IMAGE_DECLARE(Hz);    // png/Hz.png
LV_IMAGE_DECLARE(SF);    // png/SF.png
LV_IMAGE_DECLARE(BW);    // png/BW.png
LV_IMAGE_DECLARE(CR);    // png/CR.png
LV_IMAGE_DECLARE(US);    // png/US.png


void ui_init(void);
void lvgl_ptt_timer_cb(lv_timer_t *timer);
void lvgl_rtc_bat_timer_cb(lv_timer_t *timer);
void key_scan_timer_cb(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif