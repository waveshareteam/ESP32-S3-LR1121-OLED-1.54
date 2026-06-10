#include <string.h>

#include "ui.h"

// create a new instance of the HAL class
EspHal* hal = new EspHal(BSP_SPI_CLK, BSP_SPI_MISO, BSP_SPI_MOSI);

// now we can create the radio module
LR1121 radio = new Module(hal, BSP_SPI_CS, BSP_SPI_INT, BSP_SPI_RESET, BSP_SPI_BUSY);

lv_obj_t *page1;
lv_obj_t *page2;

lv_obj_t * label_rtc = NULL;
lv_obj_t * label_bat = NULL;
lv_obj_t * bat_Image = NULL;

lv_group_t * g = NULL;
lv_obj_t * ptt_btn = NULL;
lv_obj_t * label_ptt = NULL;

lv_obj_t * target_roller = NULL;

lv_obj_t * md_btn = NULL;
lv_obj_t * md_Image = NULL;
lv_obj_t * md_Roller = NULL;

lv_obj_t * hz_btn = NULL;
lv_obj_t * hz_Image = NULL;
lv_obj_t * hz_Roller = NULL;

lv_obj_t * sf_btn = NULL;
lv_obj_t * sf_Image = NULL;
lv_obj_t * sf_Roller = NULL;

lv_obj_t * bw_btn = NULL;
lv_obj_t * bw_Image = NULL;
lv_obj_t * bw_Roller = NULL;

lv_obj_t * cr_btn = NULL;
lv_obj_t * cr_Image = NULL;
lv_obj_t * cr_Roller = NULL;

lv_obj_t * us_btn = NULL;
lv_obj_t * us_Image = NULL;
lv_obj_t * us_Roller = NULL;

bool Boot_Key_Flag = false;
bool Lora_Transmission_Mode = false; // Default to receive mode
volatile radio_modem_t radio_modem = RADIO_MODEM_GFSK;
volatile bool radio_reconfigure_requested = false;
volatile float radio_frequency_mhz = 868.0f;
volatile uint8_t radio_lora_spreading_factor = 7;
volatile float radio_lora_bandwidth_khz = 125.0f;
volatile uint8_t radio_lora_coding_rate = 5;

pcf85063a_datetime_t time_info;
bsp_bat_info_t bat_info;

// Cached last values to avoid redundant radio reconfiguration
static uint8_t  last_md       = RADIO_MODEM_GFSK;
static float    last_freq_mhz = 868.0;
static uint8_t  last_sf       = 2;
static uint8_t  last_bw       = 1;
static uint8_t  last_cr       = 1;
static uint8_t  last_us       = 2;

typedef struct {
    float frequency_hz;        // Operating frequency in MHz
    int8_t tx_power_dbm;       // TX output power in dBm
    uint8_t spreading_factor;  // Spreading factor
    float bandwidth;           // Bandwidth in kHz
    uint8_t coding_rate;       // Coding rate denominator
    uint32_t pa_ramp_time;     // PA ramp time in microseconds
} lora_config_t;

lora_config_t lora_config = {
    .frequency_hz    = 868.0,  // 868MHz
    .tx_power_dbm    = 22,                 // Transmit power
    .spreading_factor= 7,
    .bandwidth       = 125.0,
    .coding_rate     = 5,
    .pa_ramp_time    = 48
};

const uint8_t sf_values[] = {
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12
};

const float bw_values[] = {
    62.5,
    125.0,
    250.0,
    500.0
};

const uint32_t pa_ramp_time_values[] = {
    16,
    32,
    48,
    64,
    80,
    96,
    112,
    128,
    144,
    160,
    176,
    192,
    208,
    240,
    272,
    304
};

char buf[256];

uint8_t current_page = 0;  


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

// Called when page transition animation finishes
// Used to focus the active roller and enable editing mode
static void anim_ready_cb(lv_anim_t * a)
{
    if (target_roller) {
        lv_group_focus_obj(target_roller);

        // Enable editing so encoder/keys control roller directly
        lv_group_set_editing(g, true);
    }
}

// Generic Y-axis animation helper
static void anim_move_y(lv_obj_t *obj, int32_t from, int32_t to, uint16_t time, lv_anim_ready_cb_t cb)
{
    lv_anim_t a;
    lv_anim_init(&a);

    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);

    if (cb) {
        lv_anim_set_ready_cb(&a, cb);
    }

    lv_anim_start(&a);
}

// Switch between main page (0) and setting page (1)
void switch_page(uint8_t target_page)
{
    if (target_page == current_page) return;

    if (target_page == 1) {
        // Slide in page2 from bottom
        lv_obj_set_y(page2, 64);
        anim_move_y(page2, 64, 0, 200, anim_ready_cb);

        // Slide out page1 to top
        anim_move_y(page1, 0, -64, 200, NULL);
    }
    else {
        // Slide in page1 from top
        lv_obj_set_y(page1, -64);
        anim_move_y(page1, -64, 0, 200, NULL);

        // Slide out page2 to bottom
        anim_move_y(page2, 0, 64, 200, NULL);

        // Exit editing mode when returning to main UI
        lv_group_set_editing(g, false);
    }

    current_page = target_page;
}

// Button event handler
static void Button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event((button_handle_t)arg);
    int button_id = (int)(intptr_t)data;

    // printf("%s :%d\r\n", iot_button_get_event_str(event), button_id);
    if (event == BUTTON_PRESS_DOWN || event == BUTTON_PRESS_UP) {
        Boot_Key_Flag = (gpio_get_level(PTT_KEY_GPIO) == 0);
    }
}

// Common exit logic after selecting a value from roller
static void roller_apply_and_exit(lv_obj_t *roller, lv_obj_t *btn)
{
    lv_obj_add_flag(roller, LV_OBJ_FLAG_HIDDEN);

    // Return focus to originating button
    lv_group_focus_obj(btn);

    // Return to main page
    switch_page(0);
}

// Open a roller and switch to setting page
static void open_roller(lv_obj_t *roller)
{
    lv_obj_remove_flag(roller, LV_OBJ_FLAG_HIDDEN);

    // Save target for focus after animation
    target_roller = roller;

    switch_page(1);
}

// Frequency selection handler
static void md_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);
    uint8_t index = lv_roller_get_selected(roller);

    if (index != last_md) {
        radio_modem = (index == 0) ? RADIO_MODEM_GFSK : RADIO_MODEM_LORA;
        radio_reconfigure_requested = true;
        lv_image_set_src(md_Image, (radio_modem == RADIO_MODEM_GFSK) ? &MD_G : &MD_L);
        last_md = index;
    }

    roller_apply_and_exit(md_Roller, md_btn);
}

// Frequency selection handler
static void hz_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    char buf[16];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));

    int freq_mhz = atoi(buf);

    // Only apply if value changed
    if (freq_mhz != last_freq_mhz) {
        lora_config.frequency_hz = freq_mhz;
        radio_frequency_mhz = (float)freq_mhz;
        peripheral_lock();
        radio.setFrequency(freq_mhz);
        peripheral_unlock();

        last_freq_mhz = freq_mhz;
    }

    roller_apply_and_exit(hz_Roller, hz_btn);
}

// Spreading factor selection handler
static void sf_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);
    uint8_t index = lv_roller_get_selected(roller);

    if (index != last_sf) {
        lora_config.spreading_factor = sf_values[index];
        radio_lora_spreading_factor = lora_config.spreading_factor;
        if (radio_modem == RADIO_MODEM_LORA) {
            peripheral_lock();
            radio.setSpreadingFactor(lora_config.spreading_factor);
            peripheral_unlock();
        }
        last_sf = index;
    }

    roller_apply_and_exit(sf_Roller, sf_btn);
}

static void bw_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    // Get current selected index
    uint8_t index = lv_roller_get_selected(roller);
    if (index != last_bw) {
        lora_config.bandwidth = bw_values[index];
        radio_lora_bandwidth_khz = lora_config.bandwidth;
        if (radio_modem == RADIO_MODEM_LORA) {
            peripheral_lock();
            radio.setBandwidth(lora_config.bandwidth);
            peripheral_unlock();
        }
        last_bw = index;
    };
    
    roller_apply_and_exit(bw_Roller, bw_btn);
}

static void cr_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    // Get current selected index
    uint8_t index = lv_roller_get_selected(roller);
    if (index != last_cr) {
        lora_config.coding_rate = index + 4;
        radio_lora_coding_rate = lora_config.coding_rate;
        if (radio_modem == RADIO_MODEM_LORA) {
            peripheral_lock();
            radio.setCodingRate(lora_config.coding_rate);
            peripheral_unlock();
        }
        last_cr = index;
    };
    
    roller_apply_and_exit(cr_Roller, cr_btn);
}

static void us_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    // Get current selected index
    uint8_t index = lv_roller_get_selected(roller);
    if (index != last_us) {
        lora_config.pa_ramp_time = pa_ramp_time_values[index];
        peripheral_lock();
        radio.setOutputPower(lora_config.tx_power_dbm, true,lora_config.pa_ramp_time);
        peripheral_unlock();
        last_us = index;
    };
    
    roller_apply_and_exit(us_Roller, us_btn);
}

// Open modem selector
static void md_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(md_Roller);
    }
}

// Open frequency selector
static void hz_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(hz_Roller);
    }
}

// Open spreading factor selector
static void sf_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(sf_Roller);
    }
}

static void bw_btn_event_cb(lv_event_t * e)
{

    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(bw_Roller);
    }
}

static void cr_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(cr_Roller);
    }
}

static void us_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(us_Roller);
    }
}

void ui_create_pages(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_text_font(scr, &lv_font_unscii_8, 0);

    /* ===== Top: Time + Battery ===== */
    label_rtc = lv_label_create(scr);
    lv_obj_align(label_rtc, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_label_set_text(label_rtc,"12:00:00");

    label_bat = lv_label_create(scr);
    lv_obj_align(label_bat, LV_ALIGN_TOP_RIGHT, -15, 0);
    lv_label_set_text(label_bat,"100%%");
    
    bat_Image = lv_image_create(scr);
    lv_obj_align(bat_Image, LV_ALIGN_TOP_RIGHT, -2, 0);
    lv_image_set_src(bat_Image, &Battery4_png);

    lv_obj_t *ui_layer = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_layer);
    lv_obj_set_size(ui_layer, 128, 64);
    lv_obj_align(ui_layer, LV_ALIGN_TOP_LEFT, 0, 0);

    /* ===== Page1 ===== */
    page1 = lv_obj_create(ui_layer);
    lv_obj_remove_style_all(page1);   // Remove default style
    lv_obj_set_size(page1, 128, 64);
    lv_obj_align(page1, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Create input device */
    lv_indev_t * indev = bsp_display_get_input_dev();

    /* Create group */
    g = lv_group_create();

    /* Bind group */
    lv_indev_set_group(indev, g);

    /* ===== PTT ===== */
    ptt_btn = lv_btn_create(page1);
    lv_obj_set_size(ptt_btn, 100, 12);
    lv_obj_align(ptt_btn, LV_ALIGN_CENTER, 0, -5);

    label_ptt = lv_label_create(ptt_btn);
    lv_obj_center(label_ptt);
    lv_label_set_text(label_ptt,"Receiving");
    
    const button_config_t btn_cfg = {0};
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = PTT_KEY_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };
    button_handle_t btn = NULL;

    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    if (ret != ESP_OK) {
        printf("Failed to create button device: %s\n", esp_err_to_name(ret));
    }

    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, Button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_PRESS_UP, NULL, Button_event_cb, NULL);

    //MD
    md_btn = lv_btn_create(page1);
    lv_obj_set_size(md_btn, 16, 16);
    lv_obj_align(md_btn, LV_ALIGN_BOTTOM_LEFT, 5, -5);

    md_Image = lv_image_create(md_btn);
    lv_obj_center(md_Image);
    lv_image_set_src(md_Image, &MD_G);

    lv_obj_add_event_cb(md_btn, md_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //Hz
    hz_btn = lv_btn_create(page1);
    lv_obj_set_size(hz_btn, 16, 16);
    lv_obj_align(hz_btn, LV_ALIGN_BOTTOM_LEFT, 26, -5);

    hz_Image = lv_image_create(hz_btn);
    lv_obj_center(hz_Image);
    lv_image_set_src(hz_Image, &Hz);

    lv_obj_add_event_cb(hz_btn, hz_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    //SF
    sf_btn = lv_btn_create(page1);
    lv_obj_set_size(sf_btn, 16, 16);
    lv_obj_align(sf_btn, LV_ALIGN_BOTTOM_LEFT, 47, -5);

    sf_Image = lv_image_create(sf_btn);
    lv_obj_center(sf_Image);
    lv_image_set_src(sf_Image, &SF);

    lv_obj_add_event_cb(sf_btn, sf_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //BW
    bw_btn = lv_btn_create(page1);
    lv_obj_set_size(bw_btn, 16, 16);
    lv_obj_align(bw_btn, LV_ALIGN_BOTTOM_LEFT, 68, -5);

    bw_Image = lv_image_create(bw_btn);
    lv_obj_center(bw_Image);
    lv_image_set_src(bw_Image, &BW);

    lv_obj_add_event_cb(bw_btn, bw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //CR
    cr_btn = lv_btn_create(page1);
    lv_obj_set_size(cr_btn, 16, 16);
    lv_obj_align(cr_btn, LV_ALIGN_BOTTOM_LEFT, 89, -5);

    cr_Image = lv_image_create(cr_btn);
    lv_obj_center(cr_Image);
    lv_image_set_src(cr_Image, &CR);

    lv_obj_add_event_cb(cr_btn, cr_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //US
    us_btn = lv_btn_create(page1);
    lv_obj_set_size(us_btn, 16, 16);
    lv_obj_align(us_btn, LV_ALIGN_BOTTOM_LEFT, 110, -5);

    us_Image = lv_image_create(us_btn);
    lv_obj_center(us_Image);
    lv_image_set_src(us_Image, &US);

    lv_obj_add_event_cb(us_btn, us_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // lv_group_add_obj(g, ptt_btn);
    lv_group_add_obj(g, md_btn);
    lv_group_add_obj(g, hz_btn);
    lv_group_add_obj(g, sf_btn);
    lv_group_add_obj(g, bw_btn);
    lv_group_add_obj(g, cr_btn);
    lv_group_add_obj(g, us_btn);
    
    /* Default focus */
    lv_group_focus_obj(md_btn);

    /******************************************* ===== Page2 ===== ***********************************************/
    page2 = lv_obj_create(ui_layer);
    lv_obj_remove_style_all(page2);
    lv_obj_set_size(page2, 128, 64);

    /* Initially placed at the bottom of the screen (hidden) */
    lv_obj_align(page2, LV_ALIGN_TOP_LEFT, 0, 64);

    //MD
    md_Roller = lv_roller_create(page2);
    lv_obj_set_size(md_Roller, 84, 40);
    lv_obj_align(md_Roller, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_roller_set_options(md_Roller,
                          "GFSK\nLoRa",
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(md_Roller, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(md_Roller, md_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(md_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, md_Roller);
    
    //Hz
    hz_Roller = lv_roller_create(page2);
    lv_obj_set_size(hz_Roller, 84, 40);
    lv_obj_align(hz_Roller, LV_ALIGN_BOTTOM_MID, 0, -10);
#if LR1121_HF
    lv_roller_set_options(hz_Roller,
                          "864\n865\n866\n867\n868\n869\n870\n871\n872\n873\n874\n875\n876\n877\n878\n879\n880\n881\n882\n883\n884\n885\n886\n887\n888\n889\n890\n891\n892\n893\n894\n895\n896\n897\n898\n899\n900\n901\n902\n903\n904\n905\n906\n907\n908\n909\n910\n911\n912\n913\n914\n915\n916\n917\n918\n919\n920\n921\n922\n923",
                          LV_ROLLER_MODE_NORMAL);
#else
    lv_roller_set_options(hz_Roller,
                        "864\n865\n866\n867\n868\n869\n870\n871\n872\n873\n874\n875\n876\n877\n878\n879\n880\n881\n882\n883\n884\n885\n886\n887\n888\n889\n890\n891\n892\n893\n894\n895\n896\n897\n898\n899\n900\n901\n902\n903\n904\n905\n906\n907\n908\n909\n910\n911\n912\n913\n914\n915\n916\n917\n918\n919\n920\n921\n922\n923",
                        LV_ROLLER_MODE_NORMAL);
#endif
    lv_roller_set_selected(hz_Roller, 4, LV_ANIM_OFF);
    lv_obj_add_event_cb(hz_Roller, hz_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(hz_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, hz_Roller);  
    
    //SF
    sf_Roller = lv_roller_create(page2);
    lv_obj_set_size(sf_Roller, 84, 40);
    lv_obj_align(sf_Roller, LV_ALIGN_BOTTOM_MID, 0, -10);
    // Roller text display
    const char *sf_options =
        "SF5\n"
        "SF6\n"
        "SF7\n"
        "SF8\n"
        "SF9\n"
        "SF10\n"
        "SF11\n"
        "SF12";    
    lv_roller_set_options(sf_Roller,
                          sf_options,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(sf_Roller, 2, LV_ANIM_OFF);
    lv_obj_add_event_cb(sf_Roller, sf_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(sf_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, sf_Roller);

    //BW
    bw_Roller = lv_roller_create(page2);
    lv_obj_set_size(bw_Roller, 84, 40);
    lv_obj_align(bw_Roller, LV_ALIGN_BOTTOM_MID, 0, -10);
    // Roller text display
    const char *bw_options =
        "62.50 kHz\n"
        "125 kHz\n"
        "250 kHz\n"
        "500 kHz";    
    lv_roller_set_options(bw_Roller,
                          bw_options,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(bw_Roller, 1, LV_ANIM_OFF);
    lv_obj_add_event_cb(bw_Roller, bw_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(bw_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, bw_Roller);
    
    //CR
    cr_Roller = lv_roller_create(page2);
    lv_obj_set_size(cr_Roller, 84, 40);
    lv_obj_align(cr_Roller, LV_ALIGN_BOTTOM_MID, 0, -10);
    // Roller text display
    const char *cr_options =
        "NO CR\n"
        "CR 45\n"
        "CR 46\n"
        "CR 47\n"
        "CR 48\n"
        "CR LI 45\n"
        "CR LI 46\n"
        "CR LI 48";    
    lv_roller_set_options(cr_Roller,
                          cr_options,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(cr_Roller, 1, LV_ANIM_OFF);
    lv_obj_add_event_cb(cr_Roller, cr_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(cr_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, cr_Roller);

    //US
    us_Roller = lv_roller_create(page2);
    lv_obj_set_size(us_Roller, 84, 40);
    lv_obj_align(us_Roller, LV_ALIGN_BOTTOM_MID, 0, -10);
    // Roller text display
    const char *us_options =
        "16 US\n"
        "32 US\n"
        "48 US\n"       
        "64 US\n"
        "80 US\n"
        "96 US\n"
        "112 US\n"
        "128 US\n"
        "144 US\n"
        "160 US\n"
        "176 US\n"
        "192 US\n"
        "208 US\n"
        "240 US\n"
        "272 US\n"
        "304 US";    
    lv_roller_set_options(us_Roller,
                          us_options,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(us_Roller, 2, LV_ANIM_OFF);
    lv_obj_add_event_cb(us_Roller, us_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(us_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, us_Roller);

}

void ui_init(void)
{
    ui_create_pages();
}

// Update PTT state label periodically
void lvgl_ptt_timer_cb(lv_timer_t *timer)
{
    lv_label_set_text(label_ptt,
        Lora_Transmission_Mode ? "Speaking" : "Receiving");
}

// Update RTC time and battery status
void lvgl_rtc_bat_timer_cb(lv_timer_t *timer)
{
    // Format time string
    snprintf(buf, sizeof(buf),
             "%02d:%02d:%02d",
             time_info.hour,
             time_info.min,
             time_info.sec);

    lv_label_set_text(label_rtc, buf);

    // Format battery percentage
    snprintf(buf, sizeof(buf), "%u%%", bat_info.soc);
    lv_label_set_text(label_bat, buf);

    // Update battery icon based on charge state
    if (bat_info.ma >= 0) {
        lv_image_set_src(bat_Image, &Charge_png); // charging
    } else {
        if (bat_info.soc > 75) {
            lv_image_set_src(bat_Image, &Battery4_png);
        } else if (bat_info.soc > 50) {
            lv_image_set_src(bat_Image, &Battery3_png);
        } else if (bat_info.soc > 25) {
            lv_image_set_src(bat_Image, &Battery2_png);
        } else {
            lv_image_set_src(bat_Image, &Battery1_png);
        }
    }
}
