#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board_extra.h"

#define FRAME_SIZE 1024
#define PRINT_SAMPLES 16
#define CHANNEL_COUNT 4

static int16_t audio_frame[FRAME_SIZE * CHANNEL_COUNT];

void app_main(void)
{
    bsp_extra_codec_init();
    bsp_extra_codec_gain_set(30, NULL);

    while (1) {
        size_t bytes_read = 0;

        bsp_extra_i2s_read(audio_frame,
                           sizeof(audio_frame),
                           &bytes_read,
                           portMAX_DELAY);

        printf("Audio L/R: ");
        for (int i = 0; i < PRINT_SAMPLES; i++) {
            int16_t L = audio_frame[i * 4 + 0];
            int16_t R = audio_frame[i * 4 + 2];

            printf("%d,%d | ", L, R);
        }
        printf("\n");
    }
}
