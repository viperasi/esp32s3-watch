#include "watch_face.h"
#include "rtc_pcf85063.h"
#include "battery.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

LV_FONT_DECLARE(font_barlow_172)
LV_FONT_DECLARE(font_barlow_120)
LV_FONT_DECLARE(font_barlow_72)
LV_FONT_DECLARE(font_barlow_38)
LV_FONT_DECLARE(font_spacemono_40)
LV_FONT_DECLARE(font_spacemono_10)

#define TAG "WATCH_FACE"

#define SCR_W 410
#define SCR_H 502
#define DW   300
#define DH   367
#define DX   ((SCR_W - DW) / 2)
#define DY   ((SCR_H - DH) / 2)

#define PAD      13
#define STRIPE_H 3
#define GRID_SP  40
#define CH_ARM   8
#define BOOT_GPIO  0
#define PWR_GPIO   10

/* Long-press threshold: 800ms */
#define LONG_PRESS_US 800000

/* Setting mode fields */
enum set_field { SET_HOUR, SET_MIN, SET_YEAR, SET_MONTH, SET_DAY, SET_FIELD_COUNT };

static const char *set_field_names[] = { "HRS", "MIN", "YEAR", "MON", "DAY" };

/* Per-theme color set */
typedef struct {
    lv_color_t bg, main, sec, tert, rev;
    lv_color_t grid;
    uint8_t    grid_opa;
    lv_color_t cross;
    lv_color_t div;
    uint8_t    div_opa;
    lv_color_t stripe_gray;
    lv_color_t bar_track;
} theme_t;

static theme_t themes[3];

static void init_themes(void)
{
    /* Light */
    themes[0] = (theme_t){
        .bg          = lv_color_hex(0xB0B0AC),
        .main        = lv_color_hex(0x1A1F2E),
        .sec         = lv_color_hex(0x555555),
        .tert        = lv_color_hex(0x888888),
        .rev         = lv_color_hex(0x777777),
        .grid        = lv_color_hex(0x000000),
        .grid_opa    = 20,
        .cross       = lv_color_hex(0x888888),
        .div         = lv_color_hex(0x000000),
        .div_opa     = 31,
        .stripe_gray = lv_color_hex(0xCCCCCC),
        .bar_track   = lv_color_hex(0xD0D0CC),
    };
    /* Dark */
    themes[1] = (theme_t){
        .bg          = lv_color_hex(0x1A1F2E),
        .main        = lv_color_hex(0xE0E0DC),
        .sec         = lv_color_hex(0xAAAAAA),
        .tert        = lv_color_hex(0x777777),
        .rev         = lv_color_hex(0x666666),
        .grid        = lv_color_hex(0xFFFFFF),
        .grid_opa    = 15,
        .cross       = lv_color_hex(0x555555),
        .div         = lv_color_hex(0xFFFFFF),
        .div_opa     = 25,
        .stripe_gray = lv_color_hex(0x3A3F4E),
        .bar_track   = lv_color_hex(0x3A3F4E),
    };
    /* Pure Black (AMOLED) */
    themes[2] = (theme_t){
        .bg          = lv_color_hex(0x000000),
        .main        = lv_color_hex(0xE0E0DC),
        .sec         = lv_color_hex(0xAAAAAA),
        .tert        = lv_color_hex(0x666666),
        .rev         = lv_color_hex(0x444444),
        .grid        = lv_color_hex(0xFFFFFF),
        .grid_opa    = 10,
        .cross       = lv_color_hex(0x333333),
        .div         = lv_color_hex(0xFFFFFF),
        .div_opa     = 20,
        .stripe_gray = lv_color_hex(0x1A1A1A),
        .bar_track   = lv_color_hex(0x1A1A1A),
    };
}

/* Accent colors stay the same in both themes */
#define COL_RED    lv_color_hex(0xC0504D)
#define COL_BLUE   lv_color_hex(0x4472C4)
#define COL_GREEN  lv_color_hex(0x4CAF50)

static watch_face_t wf;

static lv_obj_t *aod_time_label = NULL;
static bool in_aod = false;
static int aod_last_min = -1;
static int64_t aod_cooldown = 0;  /* Prevent rapid AOD re-entry */

/* Button state — accessed from ISR and LVGL timer */
static volatile int64_t boot_press_us = 0;   /* 0 = not pressed */
static volatile bool    pwr_flag = false;
static volatile int64_t last_pwr_us = 0;

/* Setting mode state (only accessed from LVGL timer context) */
static bool     setting_mode = false;
static int      setting_field = SET_HOUR;
static bool     blink_on = true;
static int      blink_cnt = 0;
static struct tm set_tm;                      /* Working copy while setting */

#define DEBOUNCE_US 200000  /* 200ms debounce for all buttons */

/* ---- Button ISR ---- */
static void IRAM_ATTR btn_isr(void *arg)
{
    int gpio = (int)(intptr_t)arg;
    int64_t now = esp_timer_get_time();

    if (gpio == BOOT_GPIO) {
        int level = gpio_get_level(BOOT_GPIO);
        if (level == 0) {
            boot_press_us = now;
        } else {
            if (boot_press_us > 0 && (now - boot_press_us) < LONG_PRESS_US) {
                boot_press_us = -1;
            }
        }
    } else if (gpio == PWR_GPIO) {
        if (gpio_get_level(PWR_GPIO) == 1 &&
            (now - last_pwr_us) > DEBOUNCE_US) {
            pwr_flag = true;
            last_pwr_us = now;
        }
    }
}

static void setup_buttons(void)
{
    /* BOOT: GPIO0, active low */
    gpio_config_t boot_io = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&boot_io);

    /* PWR: GPIO10, active high (press = high) */
    gpio_config_t pwr_io = {
        .pin_bit_mask = 1ULL << PWR_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    gpio_config(&pwr_io);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_GPIO, btn_isr, (void *)(intptr_t)BOOT_GPIO);
    gpio_isr_handler_add(PWR_GPIO,  btn_isr, (void *)(intptr_t)PWR_GPIO);
}

/* ---- Grid + crosshair draw callback ---- */
static void draw_bg_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    const theme_t *t = &themes[wf.theme_idx];

    lv_draw_rect_dsc_t gd;
    lv_draw_rect_dsc_init(&gd);
    gd.bg_color = t->grid;
    gd.bg_opa   = t->grid_opa;
    for (int y = GRID_SP; y < SCR_H; y += GRID_SP) {
        lv_area_t a = {0, y, SCR_W - 1, y};
        lv_draw_rect(layer, &gd, &a);
    }
    for (int x = GRID_SP; x < SCR_W; x += GRID_SP) {
        lv_area_t a = {x, 0, x, SCR_H - 1};
        lv_draw_rect(layer, &gd, &a);
    }

    lv_draw_rect_dsc_t cd;
    lv_draw_rect_dsc_init(&cd);
    cd.bg_color = t->cross;
    cd.bg_opa   = LV_OPA_COVER;
    int cx, cy;
    cx = DX + 16; cy = DY + 16;
    lv_draw_rect(layer, &cd, &(lv_area_t){cx-CH_ARM,cy,cx+CH_ARM,cy});
    lv_draw_rect(layer, &cd, &(lv_area_t){cx,cy-CH_ARM,cx,cy+CH_ARM});
    cx = DX + DW - 16;
    lv_draw_rect(layer, &cd, &(lv_area_t){cx-CH_ARM,cy,cx+CH_ARM,cy});
    lv_draw_rect(layer, &cd, &(lv_area_t){cx,cy-CH_ARM,cx,cy+CH_ARM});
    cx = DX + 16; cy = DY + DH - 16;
    lv_draw_rect(layer, &cd, &(lv_area_t){cx-CH_ARM,cy,cx+CH_ARM,cy});
    lv_draw_rect(layer, &cd, &(lv_area_t){cx,cy-CH_ARM,cx,cy+CH_ARM});
    cx = DX + DW - 16;
    lv_draw_rect(layer, &cd, &(lv_area_t){cx-CH_ARM,cy,cx+CH_ARM,cy});
    lv_draw_rect(layer, &cd, &(lv_area_t){cx,cy-CH_ARM,cx,cy+CH_ARM});
}

/* ---- Helpers ---- */
static lv_obj_t *make_box(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return o;
}

static lv_obj_t *make_lbl(lv_obj_t *parent, lv_color_t color,
                           const lv_font_t *font, int32_t ls)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_font(l, font, 0);
    if (ls) lv_obj_set_style_text_letter_space(l, ls, 0);
    return l;
}

/* ---- Apply current theme to all stored objects ---- */
static void apply_theme(void)
{
    const theme_t *t = &themes[wf.theme_idx];

    lv_obj_set_style_bg_color(wf.scr, t->bg, 0);
    lv_obj_invalidate(wf.scr);

    lv_obj_set_style_text_color(wf.pnl, t->sec, 0);
    lv_obj_set_style_text_color(wf.rev, t->rev, 0);
    lv_obj_set_style_text_color(wf.hrs_tag, t->tert, 0);
    lv_obj_set_style_text_color(wf.date_tag, t->tert, 0);
    lv_obj_set_style_text_color(wf.day_tag, t->tert, 0);
    lv_obj_set_style_text_color(wf.min_tag, t->tert, 0);
    lv_obj_set_style_text_color(wf.sec_tag, t->tert, 0);
    lv_obj_set_style_text_color(wf.pwr_tag, t->sec, 0);

    lv_obj_set_style_text_color(wf.hours_label, t->main, 0);
    lv_obj_set_style_text_color(wf.minutes_label, t->main, 0);
    lv_obj_set_style_text_color(wf.date_year_label, t->main, 0);
    lv_obj_set_style_text_color(wf.date_md_label, t->sec, 0);
    lv_obj_set_style_text_color(wf.bat_percent_label, t->main, 0);

    lv_obj_set_style_bg_color(wf.s1g, t->stripe_gray, 0);
    lv_obj_set_style_bg_color(wf.s2g, t->stripe_gray, 0);
    lv_obj_set_style_bg_color(wf.div1, t->div, 0);
    lv_obj_set_style_bg_opa(wf.div1, t->div_opa, 0);
    lv_obj_set_style_bg_color(wf.div2, t->div, 0);
    lv_obj_set_style_bg_opa(wf.div2, t->div_opa, 0);
    lv_obj_set_style_bg_color(wf.bat_bar, t->bar_track, LV_PART_MAIN);

    for (int i = 0; i < 4; i++)
        lv_obj_set_style_text_color(wf.status_lbl[i], t->sec, 0);
}

/* ---- Setting mode helpers ---- */

/* Increment the current field, with rollover */
static void set_field_inc(void)
{
    switch (setting_field) {
    case SET_HOUR:
        set_tm.tm_hour = (set_tm.tm_hour + 1) % 24;
        break;
    case SET_MIN:
        set_tm.tm_min = (set_tm.tm_min + 1) % 60;
        break;
    case SET_YEAR:
        set_tm.tm_year++;
        break;
    case SET_MONTH:
        set_tm.tm_mon = (set_tm.tm_mon + 1) % 12;
        break;
    case SET_DAY:
        set_tm.tm_mday = set_tm.tm_mday % 31 + 1;
        break;
    }
}

/* Save set_tm to system clock and RTC */
static void save_time(void)
{
    set_tm.tm_sec = 0;
    set_tm.tm_isdst = -1;
    time_t epoch = mktime(&set_tm);
    struct timeval tv = { .tv_sec = epoch };
    settimeofday(&tv, NULL);
    rtc_pcf85063_set_time(&set_tm);
    ESP_LOGI(TAG, "Time saved: %04d-%02d-%02d %02d:%02d",
             set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday,
             set_tm.tm_hour, set_tm.tm_min);
}

/* Show/hide a label for blink effect */
static void blink_label(lv_obj_t *lbl, bool show)
{
    const theme_t *t = &themes[wf.theme_idx];
    lv_color_t c = show ? t->main : t->bg;
    lv_obj_set_style_text_color(lbl, c, 0);
}

/* Update display labels from set_tm (setting mode) */
static void update_labels_setting(void)
{
    char buf[16];

    /* Hours — blink if selected */
    snprintf(buf, sizeof(buf), "%02d", set_tm.tm_hour);
    lv_label_set_text(wf.hours_label, buf);
    blink_label(wf.hours_label, setting_field != SET_HOUR || blink_on);

    /* Minutes — blink if selected */
    snprintf(buf, sizeof(buf), "%02d", set_tm.tm_min);
    lv_label_set_text(wf.minutes_label, buf);
    blink_label(wf.minutes_label, setting_field != SET_MIN || blink_on);

    /* Year — blink if selected */
    snprintf(buf, sizeof(buf), "%d", set_tm.tm_year + 1900);
    lv_label_set_text(wf.date_year_label, buf);
    blink_label(wf.date_year_label, setting_field != SET_YEAR || blink_on);

    /* Month.Day — blink month or day */
    snprintf(buf, sizeof(buf), "%02d.%02d", set_tm.tm_mon + 1, set_tm.tm_mday);
    lv_label_set_text(wf.date_md_label, buf);
    blink_label(wf.date_md_label,
                (setting_field != SET_MONTH && setting_field != SET_DAY) || blink_on);

    /* Day of week (auto-calculated) */
    const char *days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    lv_label_set_text(wf.day_label, days[set_tm.tm_wday]);

    /* Show which field is being edited in seconds area */
    snprintf(buf, sizeof(buf), "SET:%s", set_field_names[setting_field]);
    lv_label_set_text(wf.seconds_label, buf);
}

/* ---- AOD mode (no light sleep, timer-driven) ---- */
static void main_timer_cb(lv_timer_t *timer);  /* forward decl */
static void enter_aod_mode(void)
{
    ESP_LOGI(TAG, "Entering AOD mode");

    lv_timer_del(wf.update_timer);
    wf.update_timer = NULL;

    bsp_display_brightness_set(10);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_event_cb(scr, draw_bg_cb);
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    aod_time_label = lv_label_create(scr);
    lv_obj_set_style_text_color(aod_time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(aod_time_label, &font_barlow_120, 0);
    lv_obj_align(aod_time_label, LV_ALIGN_CENTER, 0, 0);

    struct tm tm;
    if (rtc_pcf85063_get_time(&tm)) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
        lv_label_set_text(aod_time_label, buf);
        aod_last_min = tm.tm_min;
    }

    in_aod = true;
    pwr_flag = false;

    /* AOD timer: 100ms for responsive button detection */
    wf.update_timer = lv_timer_create(main_timer_cb, 100, NULL);
}

static void exit_aod_mode(void)
{
    ESP_LOGI(TAG, "Exiting AOD mode");

    lv_timer_del(wf.update_timer);
    wf.update_timer = NULL;

    bsp_display_brightness_set(100);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_event_cb(scr, draw_bg_cb);
    lv_obj_clean(scr);

    in_aod = false;
    aod_time_label = NULL;
    aod_last_min = -1;
    pwr_flag = false;

    /* Prevent rapid re-entry for 1 second */
    aod_cooldown = esp_timer_get_time() + 1000000;

    watch_face_create();
}

void watch_face_enter_aod(void) { enter_aod_mode(); }
void watch_face_exit_aod(void) { exit_aod_mode(); }

/* ---- Unified 500ms timer ---- */
static int tick_cnt = 0;

static void main_timer_cb(lv_timer_t *timer)
{
    /* ---- AOD mode: check PWR, update time ---- */
    if (in_aod) {
        if (pwr_flag || gpio_get_level(PWR_GPIO) == 1) {
            exit_aod_mode();
            return;
        }
        struct tm tm;
        if (rtc_pcf85063_get_time(&tm)) {
            if (tm.tm_min != aod_last_min) {
                aod_last_min = tm.tm_min;
                char buf[8];
                snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
                lv_label_set_text(aod_time_label, buf);
            }
        }
        return;
    }

    tick_cnt++;

    /* ---- Handle button events ---- */
    bool long_press = false;
    bool short_press = false;

    /* Check for long press: BOOT held down */
    if (boot_press_us > 0) {
        int64_t now = esp_timer_get_time();
        if ((now - boot_press_us) >= LONG_PRESS_US && gpio_get_level(BOOT_GPIO) == 0) {
            long_press = true;
            boot_press_us = -2; /* consumed, prevent re-trigger */
        }
    }
    /* Check for short press (release detected by ISR) */
    if (boot_press_us == -1) {
        short_press = true;
        boot_press_us = 0;
    }

    /* Handle PWR button press */
    bool pwr_pressed = false;
    if (pwr_flag) {
        pwr_flag = false;
        pwr_pressed = true;
    }

    if (!setting_mode) {
        /* ---- NORMAL MODE ---- */
        if (long_press) {
            /* Enter setting mode */
            setting_mode = true;
            setting_field = SET_HOUR;
            blink_on = true;
            blink_cnt = 0;

            time_t now;
            time(&now);
            set_tm = *localtime(&now);

            ESP_LOGI(TAG, "Entered setting mode");
            update_labels_setting();
            return;
        }
        if (short_press) {
            /* Cycle theme: light → dark → pure_black → light */
            wf.theme_idx = (wf.theme_idx + 1) % 3;
            apply_theme();
            static const char *theme_names[] = {"light", "dark", "pure_black"};
            ESP_LOGI(TAG, "Theme: %s", theme_names[wf.theme_idx]);
        }

        if (pwr_pressed) {
            int64_t now = esp_timer_get_time();
            if (now >= aod_cooldown) {
                enter_aod_mode();
                return;
            }
        }

        /* Update time display every 1s (every 2nd tick) */
        if (tick_cnt % 2 == 0) {
            time_t now;
            time(&now);
            struct tm *t = localtime(&now);

            char buf[16];
            snprintf(buf, sizeof(buf), "%02d", t->tm_hour);
            lv_label_set_text(wf.hours_label, buf);
            snprintf(buf, sizeof(buf), "%02d", t->tm_min);
            lv_label_set_text(wf.minutes_label, buf);
            snprintf(buf, sizeof(buf), "%02d", t->tm_sec);
            lv_label_set_text(wf.seconds_label, buf);
            snprintf(buf, sizeof(buf), "%d", t->tm_year + 1900);
            lv_label_set_text(wf.date_year_label, buf);
            snprintf(buf, sizeof(buf), "%02d.%02d", t->tm_mon + 1, t->tm_mday);
            lv_label_set_text(wf.date_md_label, buf);
            const char *days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
            lv_label_set_text(wf.day_label, days[t->tm_wday]);
        }

        /* Update battery every 10s (every 20th tick) */
        if (tick_cnt % 20 == 0) {
            int pct = battery_get_percent();
            if (pct >= 0 && pct <= 100) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%d%%", pct);
                lv_label_set_text(wf.bat_percent_label, buf);
                lv_bar_set_value(wf.bat_bar, pct, LV_ANIM_OFF);
                lv_obj_set_style_bg_color(wf.bat_bar,
                    pct < 20 ? COL_RED : COL_BLUE, LV_PART_INDICATOR);
            }
        }
    } else {
        /* ---- SETTING MODE ---- */
        if (long_press) {
            /* Exit setting mode, save time */
            save_time();
            setting_mode = false;
            /* Restore all label colors */
            apply_theme();
            /* Immediately update time display */
            time_t now;
            time(&now);
            struct tm *t = localtime(&now);
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d", t->tm_hour);
            lv_label_set_text(wf.hours_label, buf);
            snprintf(buf, sizeof(buf), "%02d", t->tm_min);
            lv_label_set_text(wf.minutes_label, buf);
            snprintf(buf, sizeof(buf), "%02d", t->tm_sec);
            lv_label_set_text(wf.seconds_label, buf);
            snprintf(buf, sizeof(buf), "%d", t->tm_year + 1900);
            lv_label_set_text(wf.date_year_label, buf);
            snprintf(buf, sizeof(buf), "%02d.%02d", t->tm_mon + 1, t->tm_mday);
            lv_label_set_text(wf.date_md_label, buf);
            const char *days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
            lv_label_set_text(wf.day_label, days[t->tm_wday]);
            ESP_LOGI(TAG, "Exited setting mode");
            return;
        }

        if (short_press) {
            /* Cycle to next field */
            setting_field = (setting_field + 1) % SET_FIELD_COUNT;
            blink_on = true;
            blink_cnt = 0;
        }
        if (pwr_pressed) {
            /* Increment current field */
            set_field_inc();
            blink_on = true;
            blink_cnt = 0;
        }

        /* Blink toggle every tick */
        blink_cnt++;
        blink_on = (blink_cnt % 2 == 0);

        update_labels_setting();
    }
}

/* ================================================================== */
void watch_face_create(void)
{
    init_themes();
    setup_buttons();
    wf.theme_idx = 0;

    lv_obj_t *scr = lv_screen_active();
    const theme_t *t = &themes[0];
    lv_obj_set_style_bg_color(scr, t->bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(scr, draw_bg_cb, LV_EVENT_DRAW_MAIN, NULL);
    wf.scr = scr;

    /* 300x367 container, centered */
    lv_obj_t *c = make_box(scr, DW, DH);
    lv_obj_set_pos(c, DX, DY);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(c, 0, 0);

    /* ===== TOP BAR ===== */
    lv_obj_t *top_bar = make_box(c, DW, 20);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(top_bar, PAD, 0);
    lv_obj_set_style_pad_top(top_bar, 6, 0);

    wf.pnl = make_lbl(top_bar, t->sec, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.pnl, "PNL-042 \xC2\xB7 WATCH");
    wf.rev = make_lbl(top_bar, t->rev, &lv_font_montserrat_14, 1);
    lv_label_set_text(wf.rev, "REV.2026.06");

    /* ===== STRIPE 1 ===== */
    lv_obj_t *s1 = make_box(c, DW, STRIPE_H);
    lv_obj_set_flex_flow(s1, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s1, 4, 0);
    lv_obj_set_style_pad_hor(s1, PAD, 0);
    lv_obj_t *s1r = make_box(s1, 66, STRIPE_H);
    lv_obj_set_style_bg_color(s1r, COL_RED, 0);
    lv_obj_set_style_bg_opa(s1r, LV_OPA_COVER, 0);
    wf.s1g = lv_obj_create(s1);
    lv_obj_remove_style_all(wf.s1g);
    lv_obj_set_flex_grow(wf.s1g, 1);
    lv_obj_set_style_bg_color(wf.s1g, t->stripe_gray, 0);
    lv_obj_set_style_bg_opa(wf.s1g, LV_OPA_COVER, 0);
    lv_obj_t *s1b = make_box(s1, 80, STRIPE_H);
    lv_obj_set_style_bg_color(s1b, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(s1b, LV_OPA_COVER, 0);

    /* ===== HOURS ROW ===== */
    lv_obj_t *hours_row = lv_obj_create(c);
    lv_obj_remove_style_all(hours_row);
    lv_obj_set_width(hours_row, DW);
    lv_obj_set_flex_grow(hours_row, 115);
    lv_obj_add_flag(hours_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(hours_row, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== DIVIDER 1 ===== */
    wf.div1 = make_box(c, DW - 2 * PAD, 1);
    lv_obj_set_style_bg_color(wf.div1, t->div, 0);
    lv_obj_set_style_bg_opa(wf.div1, t->div_opa, 0);

    /* ===== MINUTES ROW ===== */
    lv_obj_t *minutes_row = lv_obj_create(c);
    lv_obj_remove_style_all(minutes_row);
    lv_obj_set_width(minutes_row, DW);
    lv_obj_set_flex_grow(minutes_row, 105);
    lv_obj_add_flag(minutes_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(minutes_row, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== DIVIDER 2 ===== */
    wf.div2 = make_box(c, DW - 2 * PAD, 1);
    lv_obj_set_style_bg_color(wf.div2, t->div, 0);
    lv_obj_set_style_bg_opa(wf.div2, t->div_opa, 0);

    /* ===== BOTTOM ROW ===== */
    lv_obj_t *bottom_row = make_box(c, DW, 44);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(bottom_row, PAD, 0);
    lv_obj_set_style_pad_column(bottom_row, PAD, 0);

    lv_obj_t *sec_area = make_box(bottom_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sec_area, LV_FLEX_FLOW_COLUMN);
    wf.sec_tag = make_lbl(sec_area, t->tert, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.sec_tag, "SEC");
    wf.seconds_label = make_lbl(sec_area, COL_RED, &font_spacemono_40, -1);
    lv_label_set_text(wf.seconds_label, "25");

    lv_obj_t *bat_area = lv_obj_create(bottom_row);
    lv_obj_remove_style_all(bat_area);
    lv_obj_set_flex_grow(bat_area, 1);
    lv_obj_set_height(bat_area, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bat_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bat_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                           LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(bat_area, 3, 0);
    lv_obj_clear_flag(bat_area, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bat_top = make_box(bat_area, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bat_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bat_top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    wf.pwr_tag = make_lbl(bat_top, t->sec, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.pwr_tag, "PWR SYS");
    wf.bat_percent_label = make_lbl(bat_top, t->main, &lv_font_montserrat_14, 1);
    {
        int init_pct = battery_get_percent();
        if (init_pct < 0) init_pct = 0;
        if (init_pct > 100) init_pct = 100;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", init_pct);
        lv_label_set_text(wf.bat_percent_label, buf);
    }

    wf.bat_bar = lv_bar_create(bat_area);
    lv_obj_set_size(wf.bat_bar, LV_PCT(100), 5);
    lv_bar_set_range(wf.bat_bar, 0, 100);
    {
        int init_pct = battery_get_percent();
        if (init_pct < 0) init_pct = 0;
        if (init_pct > 100) init_pct = 100;
        lv_bar_set_value(wf.bat_bar, init_pct, LV_ANIM_OFF);
    }
    lv_obj_set_style_bg_color(wf.bat_bar, t->bar_track, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wf.bat_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(wf.bat_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_color(wf.bat_bar, COL_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(wf.bat_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(wf.bat_bar, 1, LV_PART_INDICATOR);

    /* ===== STRIPE 2 ===== */
    lv_obj_t *s2 = make_box(c, DW, STRIPE_H);
    lv_obj_set_flex_flow(s2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s2, 4, 0);
    lv_obj_set_style_pad_hor(s2, PAD, 0);
    lv_obj_t *s2r = make_box(s2, 58, STRIPE_H);
    lv_obj_set_style_bg_color(s2r, COL_RED, 0);
    lv_obj_set_style_bg_opa(s2r, LV_OPA_COVER, 0);
    wf.s2g = lv_obj_create(s2);
    lv_obj_remove_style_all(wf.s2g);
    lv_obj_set_flex_grow(wf.s2g, 1);
    lv_obj_set_style_bg_color(wf.s2g, t->stripe_gray, 0);
    lv_obj_set_style_bg_opa(wf.s2g, LV_OPA_COVER, 0);
    lv_obj_t *s2b = make_box(s2, 73, STRIPE_H);
    lv_obj_set_style_bg_color(s2b, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(s2b, LV_OPA_COVER, 0);

    /* ===== STATUS BAR ===== */
    lv_obj_t *status_bar = make_box(c, DW, 14);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(status_bar, PAD, 0);

    const struct { const char *t; lv_color_t c; bool dot; } st[] = {
        {"SYS",     COL_GREEN, true},
        {"NAV",     COL_BLUE,  true},
        {"COM",     COL_RED,   true},
        {"MEM 64K", t->tert,   false},
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *item = lv_obj_create(status_bar);
        lv_obj_remove_style_all(item);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START,
                               LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(item, 3, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        if (st[i].dot) {
            lv_obj_t *dot = make_box(item, 4, 4);
            lv_obj_set_style_bg_color(dot, st[i].c, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(dot, 2, 0);
        }
        wf.status_lbl[i] = make_lbl(item, t->sec, &lv_font_montserrat_14, 1);
        lv_label_set_text(wf.status_lbl[i], st[i].t);
    }

    lv_obj_update_layout(c);

    /* ===== HOURS ROW CONTENT ===== */
    wf.hours_label = make_lbl(hours_row, t->main, &font_barlow_172, -8);
    lv_label_set_text(wf.hours_label, "21");
    lv_obj_align(wf.hours_label, LV_ALIGN_TOP_LEFT, 6, 10);

    wf.hrs_tag = make_lbl(hours_row, t->tert, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.hrs_tag, "HRS");
    lv_obj_align(wf.hrs_tag, LV_ALIGN_TOP_LEFT, PAD, 2);

    lv_obj_t *date_area = make_box(hours_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(date_area, LV_ALIGN_TOP_RIGHT, -PAD, 10);
    lv_obj_set_flex_flow(date_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(date_area, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END,
                           LV_FLEX_ALIGN_END);

    wf.date_tag = make_lbl(date_area, t->tert, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.date_tag, "DATE");
    lv_obj_set_style_pad_bottom(wf.date_tag, 1, 0);

    wf.date_year_label = make_lbl(date_area, t->main, &font_barlow_38, 1);
    lv_label_set_text(wf.date_year_label, "2026");

    wf.date_md_label = make_lbl(date_area, t->sec, &font_barlow_38, 1);
    lv_label_set_text(wf.date_md_label, "06.05");

    /* ===== MINUTES ROW CONTENT ===== */
    lv_obj_t *day_area = make_box(minutes_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(day_area, LV_ALIGN_BOTTOM_LEFT, PAD, -10);
    lv_obj_set_flex_flow(day_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(day_area, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    wf.day_tag = make_lbl(day_area, t->tert, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.day_tag, "DAY");
    lv_obj_set_style_pad_bottom(wf.day_tag, 1, 0);

    wf.day_label = make_lbl(day_area, COL_BLUE, &font_barlow_72, 2);
    lv_label_set_text(wf.day_label, "FRI");

    wf.minutes_label = make_lbl(minutes_row, t->main, &font_barlow_172, -8);
    lv_label_set_text(wf.minutes_label, "32");
    lv_obj_align(wf.minutes_label, LV_ALIGN_BOTTOM_RIGHT, -6, -10);

    wf.min_tag = make_lbl(minutes_row, t->tert, &lv_font_montserrat_14, 2);
    lv_label_set_text(wf.min_tag, "MINUTES");
    lv_obj_align(wf.min_tag, LV_ALIGN_BOTTOM_RIGHT, -PAD, -3);

    /* Start unified 500ms timer */
    tick_cnt = 1;  /* so first main_timer_cb sees tick_cnt=2 (even) and updates time */
    main_timer_cb(NULL);
    wf.update_timer = lv_timer_create(main_timer_cb, 500, NULL);

    ESP_LOGI(TAG, "Watch face created (300x367, BOOT=theme/setting, PWR=increment)");
}
