#pragma once

#include "lvgl.h"
#include <stdbool.h>

typedef struct {
    /* Dynamic labels (updated every second) */
    lv_obj_t *hours_label;
    lv_obj_t *minutes_label;
    lv_obj_t *date_year_label;
    lv_obj_t *date_md_label;
    lv_obj_t *day_label;
    lv_obj_t *seconds_label;
    lv_obj_t *bat_percent_label;
    lv_obj_t *bat_bar;
    lv_timer_t *update_timer;

    /* Objects that change color with theme */
    lv_obj_t *scr;
    lv_obj_t *pnl;
    lv_obj_t *rev;
    lv_obj_t *hrs_tag;
    lv_obj_t *date_tag;
    lv_obj_t *day_tag;
    lv_obj_t *min_tag;
    lv_obj_t *sec_tag;
    lv_obj_t *pwr_tag;
    lv_obj_t *s1g;
    lv_obj_t *s2g;
    lv_obj_t *div1;
    lv_obj_t *div2;
    lv_obj_t *status_lbl[4];

    int theme_idx;  /* 0=light, 1=dark, 2=pure_black */
} watch_face_t;

void watch_face_create(void);
void watch_face_enter_aod(void);
void watch_face_exit_aod(void);
