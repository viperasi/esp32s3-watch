#include "app_launcher.h"
#include "app_manager.h"
#include "lvgl.h"
#include "esp_log.h"

#define TAG "LAUNCHER"

#define COLS 3
#define ROWS 3
#define PAD  8
#define TOP  50
#define BOT  30

/* Usable display area */
#define VIS_W 310
#define VIS_H 380
#define SCR_W 410
#define SCR_H 502
#define OFF_X ((SCR_W - VIS_W) / 2)
#define OFF_Y ((SCR_H - VIS_H) / 2)

static void gesture_cb(lv_event_t *e)
{
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_BOTTOM) {
        app_manager_back();
    }
}

static void app_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    app_manager_open_app(idx);
}

lv_obj_t *app_launcher_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    /* Centered container for the 310×380 usable area */
    lv_obj_t *area = lv_obj_create(scr);
    lv_obj_remove_style_all(area);
    lv_obj_set_size(area, VIS_W, VIS_H);
    lv_obj_set_pos(area, OFF_X, OFF_Y);
    lv_obj_clear_flag(area, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(area);
    lv_label_set_text(title, "APPS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 0);

    /* Calculate cell dimensions */
    int grid_w = VIS_W - PAD * 2;
    int grid_h = VIS_H - TOP - BOT;
    int cell_w = (grid_w - (COLS - 1) * PAD) / COLS;
    int cell_h = (grid_h - PAD * 2 - (ROWS - 1) * PAD) / ROWS;

    /* Grid container */
    lv_obj_t *grid = lv_obj_create(area);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, VIS_W, grid_h);
    lv_obj_set_pos(grid, 0, TOP);
    lv_obj_set_style_pad_column(grid, PAD, 0);
    lv_obj_set_style_pad_row(grid, PAD, 0);
    lv_obj_set_style_pad_all(grid, PAD, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLL_ELASTIC);

    int count = app_manager_get_app_count();
    int cells = count < COLS * ROWS ? COLS * ROWS : count;

    for (int i = 0; i < cells; i++) {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_set_size(btn, cell_w, cell_h);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1F2E), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *content = lv_obj_create(btn);
        lv_obj_remove_style_all(content);
        lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        if (i < count) {
            lv_obj_add_event_cb(btn, app_click_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);

            lv_obj_t *icon = lv_label_create(content);
            lv_label_set_text(icon, app_manager_get_app_symbol(i));
            lv_obj_set_style_text_color(icon, lv_color_hex(0xE0E0DC), 0);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);

            lv_obj_t *name = lv_label_create(content);
            lv_label_set_text(name, app_manager_get_app_name(i));
            lv_obj_set_style_text_color(name, lv_color_hex(0x999999), 0);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        }
    }

    /* Bottom hint */
    lv_obj_t *hint = lv_label_create(area);
    lv_label_set_text(hint, "Swipe down / BOOT to close");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

    return scr;
}
