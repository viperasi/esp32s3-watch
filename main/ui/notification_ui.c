#include "notification_ui.h"
#include "ble/ble_gadgetbridge.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define TAG "NOTIF_UI"

LV_FONT_DECLARE(font_barlow_38)
LV_FONT_DECLARE(font_chinese_16)

#define CARD_W     380
#define CARD_H     200
#define CARD_RADIUS 16
#define AUTO_DISMISS_MS 5000

static lv_obj_t *card = NULL;
static lv_timer_t *auto_timer = NULL;
static int active_id = -1;

static void dismiss_task(void *arg)
{
    int id = (int)(intptr_t)arg;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"t\":\"notify\",\"id\":%d,\"n\":\"DISMISS\"}", id);
    ble_gadgetbridge_send(buf);
    vTaskDelete(NULL);
}

static void send_dismiss_deferred(int id)
{
    xTaskCreate(dismiss_task, "dismiss", 4096, (void *)(intptr_t)id, 5, NULL);
}

static void close_card(void)
{
    if (auto_timer) {
        lv_timer_del(auto_timer);
        auto_timer = NULL;
    }
    if (card) {
        lv_obj_del(card);
        card = NULL;
    }
    active_id = -1;
}

static void auto_timer_cb(lv_timer_t *timer)
{
    int id = active_id;
    close_card();
    if (id >= 0) send_dismiss_deferred(id);
}

static void card_click_cb(lv_event_t *e)
{
    (void)e;
    int id = active_id;
    close_card();
    if (id >= 0) send_dismiss_deferred(id);
}

void notification_ui_show(const char *src, const char *title, const char *body, int id)
{
    if (card) close_card();

    active_id = id;

    lv_obj_t *scr = lv_screen_active();
    card = lv_obj_create(scr);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, card_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cont = lv_obj_create(card);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 6, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *src_lbl = lv_label_create(cont);
    lv_obj_set_style_text_color(src_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(src_lbl, &font_chinese_16, 0);
    lv_label_set_text(src_lbl, src ? src : "");

    lv_obj_t *title_lbl = lv_label_create(cont);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_lbl, &font_chinese_16, 0);
    lv_label_set_text(title_lbl, title ? title : "");

    lv_obj_t *body_lbl = lv_label_create(cont);
    lv_obj_set_style_text_color(body_lbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(body_lbl, &font_chinese_16, 0);
    lv_label_set_text(body_lbl, body ? body : "");
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body_lbl, CARD_W - 32);

    auto_timer = lv_timer_create(auto_timer_cb, AUTO_DISMISS_MS, NULL);
    lv_timer_set_repeat_count(auto_timer, 1);

    ESP_LOGI(TAG, "Notification shown id=%d src=%s", id, src ? src : "");
}

void notification_ui_dismiss(int id)
{
    if (card && id == active_id) {
        close_card();
        ESP_LOGI(TAG, "Notification dismissed id=%d", id);
    }
}

void notification_ui_init(void)
{
    ESP_LOGI(TAG, "Notification UI initialized");
}
