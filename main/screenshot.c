#include "screenshot.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define TAG "SCREENSHOT"

static bool s_auto_capture = false;
static int s_capture_delay_sec = 0;

static void do_screenshot(void)
{
    if (!bsp_display_lock(5000)) {
        ESP_LOGE(TAG, "Failed to lock display");
        return;
    }

    lv_obj_t *scr = lv_screen_active();
    if (!scr) {
        bsp_display_unlock();
        ESP_LOGE(TAG, "No active screen");
        return;
    }

    lv_draw_buf_t *buf = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565);
    bsp_display_unlock();

    if (!buf) {
        ESP_LOGE(TAG, "Snapshot failed");
        return;
    }

    uint16_t ow = (uint16_t)buf->header.w;
    uint16_t oh = (uint16_t)buf->header.h;
    uint32_t stride = buf->header.stride;

    // Downsample 2x to reduce transfer size
    uint16_t w = ow / 2;
    uint16_t h = oh / 2;
    size_t row_bytes = w * 2;
    uint8_t *row_buf = malloc(row_bytes);
    if (!row_buf) {
        ESP_LOGE(TAG, "No memory for row buffer");
        lv_draw_buf_destroy(buf);
        return;
    }

    ESP_LOGI(TAG, "Captured %dx%d -> %dx%d", ow, oh, w, h);

    printf("<<<SCR_START:W=%d:H=%d>>>\n", w, h);
    fflush(stdout);

    uint8_t *src = buf->data;
    for (int y = 0; y < h; y++) {
        uint8_t *s = src + (y * 2) * stride;
        for (int x = 0; x < w; x++) {
            int sx = x * 2;
            row_buf[x * 2]     = s[sx * 2];
            row_buf[x * 2 + 1] = s[sx * 2 + 1];
        }
        // Hex encode each row to avoid VFS binary issues
        for (int i = 0; i < row_bytes; i++) {
            printf("%02x", row_buf[i]);
        }
        printf("\n");
    }
    fflush(stdout);

    printf("<<<SCR_END>>>\n");
    fflush(stdout);

    free(row_buf);
    lv_draw_buf_destroy(buf);

    ESP_LOGI(TAG, "Done");
}

static void screenshot_task(void *arg)
{
    ESP_LOGI(TAG, "Task started%s", s_auto_capture ? " (auto capture)" : "");

    if (s_auto_capture && s_capture_delay_sec > 0) {
        ESP_LOGI(TAG, "Auto capture in %d seconds...", s_capture_delay_sec);
        vTaskDelay(pdMS_TO_TICKS(s_capture_delay_sec * 1000));
        do_screenshot();
    }

    vTaskDelete(NULL);
}

void screenshot_init(void)
{
    s_auto_capture = false;
    s_capture_delay_sec = 0;
    xTaskCreatePinnedToCore(screenshot_task, "screenshot", 4096, NULL, 1, NULL, 1);
}

void screenshot_auto_capture(int delay_sec)
{
    s_auto_capture = true;
    s_capture_delay_sec = delay_sec;
    xTaskCreatePinnedToCore(screenshot_task, "screenshot", 8192, NULL, 1, NULL, 1);
}
