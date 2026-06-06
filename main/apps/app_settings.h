#pragma once

#include "lvgl.h"

lv_obj_t *app_settings_create(void);
void app_settings_update_wifi_status(const char *text);
void app_settings_update_bt_status(const char *text);
