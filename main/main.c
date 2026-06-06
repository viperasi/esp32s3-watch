#include "esp_log.h"
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "watch_face.h"
#include "app_manager.h"
#include "rtc_pcf85063.h"
#include "battery.h"

#define TAG "WATCH"

static void set_time_from_build(void)
{
    char month_str[4] = {0};
    int year, day, hour, min, sec;

    sscanf(__DATE__, "%3s %d %d", month_str, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

    const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int month = 0;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_str, months[i]) == 0) { month = i; break; }
    }

    struct tm tm_build = {0};
    tm_build.tm_year = year - 1900;
    tm_build.tm_mon = month;
    tm_build.tm_mday = day;
    tm_build.tm_hour = hour;
    tm_build.tm_min = min;
    tm_build.tm_sec = sec;
    tm_build.tm_isdst = -1;

    time_t epoch = mktime(&tm_build);
    struct timeval tv = { .tv_sec = epoch };
    settimeofday(&tv, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Pragmata Watch...");

    /* Set timezone to UTC+8 (China Standard Time) */
    setenv("TZ", "CST-8", 1);
    tzset();

    /* Set system time from build timestamp */
    set_time_from_build();

    bsp_display_start();

    /* Initialize app manager */
    app_manager_init();

    /* Initialize RTC chip */
    rtc_pcf85063_init();

    /* Initialize battery (AXP2101 PMIC) */
    battery_init();

    /* Try reading time from RTC; fall back to build timestamp */
    struct tm rtc_tm;
    if (rtc_pcf85063_get_time(&rtc_tm)) {
        time_t epoch = mktime(&rtc_tm);
        struct timeval tv = { .tv_sec = epoch };
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "System time set from RTC");
    } else {
        set_time_from_build();
        ESP_LOGI(TAG, "RTC invalid, using build timestamp");
    }

    bsp_display_lock(0);
    watch_face_create();
    bsp_display_unlock();

    ESP_LOGI(TAG, "Watch face ready");
}
