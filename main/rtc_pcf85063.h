#pragma once

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

esp_err_t rtc_pcf85063_init(void);
bool rtc_pcf85063_get_time(struct tm *out);
void rtc_pcf85063_set_time(const struct tm *t);
void rtc_pcf85063_enable_minute_interrupt(void);
void rtc_pcf85063_disable_minute_interrupt(void);
