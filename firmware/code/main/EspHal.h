/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP_HAL_H
#define ESP_HAL_H

#include <stdio.h>

#include <RadioLib.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

// define Arduino-style macros
#define LOW                         (0x0)
#define HIGH                        (0x1)
#define INPUT                       (0x01)
#define OUTPUT                      (0x03)
#define RISING                      (0x01)
#define FALLING                     (0x02)
#define NOP()                       asm volatile ("nop")


// Minimal ESP-IDF RadioLib HAL for the LR1121 SPI and GPIO control pins.
class EspHal : public RadioLibHal {
public:

    EspHal(int8_t sck, int8_t miso, int8_t mosi)
        : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
          spiSCK(sck), spiMISO(miso), spiMOSI(mosi), spi(nullptr), spiBusOwner(false) {}

    void init() override {
        spiBegin();
    }

    void term() override {
        spiEnd();
    }

    void pinMode(uint32_t pin, uint32_t mode) override {

        if(pin == RADIOLIB_NC) return;

        gpio_config_t conf = {};
        conf.pin_bit_mask = 1ULL << pin;

        if(mode == OUTPUT)
            conf.mode = GPIO_MODE_OUTPUT;
        else
        { 
            conf.mode = GPIO_MODE_INPUT;
            // conf.pull_up_en = GPIO_PULLUP_ENABLE; 
        }
            

        gpio_config(&conf);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {

        if(pin == RADIOLIB_NC) return;

        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {

        if(pin == RADIOLIB_NC) return 0;

        return gpio_get_level((gpio_num_t)pin);
    }

    void attachInterrupt(uint32_t pin, void (*cb)(void), uint32_t mode) override {

        if(pin == RADIOLIB_NC) return;

        gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)mode);

        // RadioLib may attach repeatedly; install the shared GPIO ISR service once.
        static bool isrServiceInstalled = false;
        if (!isrServiceInstalled) {
            esp_err_t err = gpio_install_isr_service(0);
            if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
                isrServiceInstalled = true;
            } else {
                ESP_LOGE("EspHal", "gpio_install_isr_service failed: %s", esp_err_to_name(err));
                return;
            }
        }

        gpio_isr_handler_remove((gpio_num_t)pin);
        gpio_isr_handler_add((gpio_num_t)pin, (gpio_isr_t)cb, NULL);
    }

    void detachInterrupt(uint32_t pin) override {

        if(pin == RADIOLIB_NC) return;

        gpio_intr_disable((gpio_num_t)pin);
        gpio_isr_handler_remove((gpio_num_t)pin);
        gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    }

    void delay(unsigned long ms) override {

        vTaskDelay(ms / portTICK_PERIOD_MS);
    }

    void delayMicroseconds(unsigned long us) override {

        uint64_t start = esp_timer_get_time();

        while((esp_timer_get_time() - start) < us);
    }

    unsigned long millis() override {

        return esp_timer_get_time() / 1000;
    }

    unsigned long micros() override {

        return esp_timer_get_time();
    }

    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override {
        if(pin == RADIOLIB_NC) {
            return 0;
        }

        pinMode(pin, INPUT);

        uint32_t start = micros();

        // Wait until the pin leaves the requested state.
        while(digitalRead(pin) == state) {
            if((micros() - start) > timeout) {
                return 0;
            }
        }

        uint32_t pulseStart = micros();

        while(digitalRead(pin) != state) {
            if((micros() - pulseStart) > timeout) {
                return 0;
            }
        }

        return micros() - pulseStart;
    }

    // Reuse an already initialized BSP SPI bus, but remember ownership when we create it.
    void spiBegin() {

        if(spi != nullptr) {
            return;
        }

        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = spiSCK;
        buscfg.mosi_io_num = spiMOSI;
        buscfg.miso_io_num = spiMISO;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 255;

        printf("spiSCK: %d, spiMISO: %d, spiMOSI: %d\n", spiSCK, spiMISO, spiMOSI);
        esp_err_t err = spi_bus_initialize(BSP_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO);
        if(err == ESP_OK) {
            spiBusOwner = true;
        } else if(err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE("EspHal", "spi_bus_initialize failed: %s", esp_err_to_name(err));
            return;
        }

        spi_host_device_t host = BSP_SPI_NUM;

        spi_device_interface_config_t devcfg = {};
        devcfg.clock_speed_hz = BSP_SPI_CLK_SPEED_HZ;
        devcfg.mode = 0;
        devcfg.spics_io_num = -1;
        devcfg.queue_size = 1;

        err = spi_bus_add_device(host, &devcfg, &spi);
        if(err != ESP_OK) {
            ESP_LOGE("EspHal", "spi_bus_add_device failed: %s", esp_err_to_name(err));
            spi = nullptr;
            // Free the SPI bus only when this HAL instance initialized it.
            if(spiBusOwner) {
                spi_bus_free(BSP_SPI_NUM);
                spiBusOwner = false;
            }
        }
    }

    void spiBeginTransaction() {}

    uint8_t spiTransferByte(uint8_t b) {

        spi_transaction_t t = {};
        t.length = 8;
        t.tx_buffer = &b;

        uint8_t rx;
        t.rx_buffer = &rx;

        spi_device_transmit(spi, &t);

        return rx;
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) {

        spi_transaction_t t = {};
        t.length = len * 8;
        t.tx_buffer = out;
        t.rx_buffer = in;

        spi_device_transmit(spi, &t);
    }

    void spiEndTransaction() {}

    void spiEnd() {

        if(spi != nullptr) {
            spi_bus_remove_device(spi);
            spi = nullptr;
        }

        if(spiBusOwner) {
            spi_bus_free(BSP_SPI_NUM);
            spiBusOwner = false;
        }

    }

private:

    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;

    spi_device_handle_t spi;
    bool spiBusOwner;
};
#endif
