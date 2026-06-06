#pragma once

#include <stdbool.h>
#include "lvgl.h"

void app_manager_init(void);
void app_manager_show_launcher(void);
void app_manager_open_app(int index);
void app_manager_back(void);
bool app_manager_is_foreground(void);
int app_manager_get_app_count(void);
const char *app_manager_get_app_name(int idx);
const char *app_manager_get_app_symbol(int idx);

/* Sub-screen navigation (within an app) */
void app_manager_push_screen(lv_obj_t *scr);
void app_manager_pop_screen(void);
