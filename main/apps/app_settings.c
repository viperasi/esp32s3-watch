#include "app_settings.h"
#include "app_manager.h"
#include "lvgl.h"
#include "esp_log.h"

#define TAG "APP_SETTINGS"
#define SCR_W 410
#define SCR_H 502

LV_FONT_DECLARE(font_barlow_56)
LV_FONT_DECLARE(font_barlow_38)

static lv_obj_t *wifi_status_label = NULL;
static lv_obj_t *bt_status_label = NULL;

static void gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_BOTTOM) {
        app_manager_back();
    }
}

static void wifi_click_cb(lv_event_t *e)
{
    /* Placeholder: push a "coming soon" sub-screen */
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0E0DC), 0);
    lv_obj_set_style_text_font(title, &font_barlow_56, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *ph = lv_label_create(scr);
    lv_label_set_text(ph, "Coming soon...");
    lv_obj_set_style_text_color(ph, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(ph, &lv_font_montserrat_14, 0);
    lv_obj_align(ph, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Swipe down / BOOT to go back");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* Gesture on sub-screen */
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    app_manager_push_screen(scr);
}

static void bt_click_cb(lv_event_t *e)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Bluetooth");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0E0DC), 0);
    lv_obj_set_style_text_font(title, &font_barlow_56, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *ph = lv_label_create(scr);
    lv_label_set_text(ph, "Coming soon...");
    lv_obj_set_style_text_color(ph, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(ph, &lv_font_montserrat_14, 0);
    lv_obj_align(ph, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Swipe down / BOOT to go back");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    app_manager_push_screen(scr);
}

void app_settings_update_wifi_status(const char *text)
{
    if (wifi_status_label) {
        lv_label_set_text(wifi_status_label, text);
    }
}

void app_settings_update_bt_status(const char *text)
{
    if (bt_status_label) {
        lv_label_set_text(bt_status_label, text);
    }
}

static lv_obj_t *create_menu_item(lv_obj_t *parent, const char *symbol,
                                   const char *name, lv_event_cb_t click_cb)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_size(item, SCR_W - 40, 80);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x1A1F2E), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(item, 12, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 10, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *icon = lv_label_create(item);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xE0E0DC), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *lbl = lv_label_create(item);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xE0E0DC), 0);
    lv_obj_set_style_text_font(lbl, &font_barlow_38, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 50, -8);

    lv_obj_t *arrow = lv_label_create(item);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_20, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);

    return item;
}

static lv_obj_t *create_status_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 50, 14);
    return lbl;
}

lv_obj_t *app_settings_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE0E0DC), 0);
    lv_obj_set_style_text_font(title, &font_barlow_56, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* Scrollable menu container */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, SCR_W, SCR_H - 140);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(cont, 10, 0);

    /* WiFi item */
    lv_obj_t *wifi_item = create_menu_item(cont, LV_SYMBOL_WIFI, "WiFi", wifi_click_cb);
    wifi_status_label = create_status_label(wifi_item, "Not connected");

    /* Bluetooth item */
    lv_obj_t *bt_item = create_menu_item(cont, LV_SYMBOL_BLUETOOTH, "Bluetooth", bt_click_cb);
    bt_status_label = create_status_label(bt_item, "Off");

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Swipe down / BOOT to go back");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    ESP_LOGI(TAG, "Settings app created");
    return scr;
}
