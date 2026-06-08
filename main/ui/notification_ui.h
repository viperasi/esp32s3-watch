#pragma once

#include "lvgl.h"

void notification_ui_init(void);
void notification_ui_show(const char *src, const char *title, const char *body, int id);
void notification_ui_dismiss(int id);
