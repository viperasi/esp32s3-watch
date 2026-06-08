#include "esp_log.h"
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "watch_face.h"
#include "app_manager.h"
#include "rtc_pcf85063.h"
#include "battery.h"
#include "ble_manager.h"
#include "ble/ble_gadgetbridge.h"
#include "ui/notification_ui.h"

#include "alarm_manager.h"
#include "nvs_flash.h"

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

/* Heap-allocated structs for passing data across threads via lv_async_call */
typedef struct {
    int id;
    char src[32];
    char title[64];
    char body[256];
} notify_async_t;


static void show_notify_async(void *user_data)
{
    notify_async_t *d = (notify_async_t *)user_data;
    notification_ui_show(d->src, d->title, d->body, d->id);
    free(d);
}

static void dismiss_notify_async(void *user_data)
{
    int id = (int)(intptr_t)user_data;
    notification_ui_dismiss(id);
}

static void on_notify_received_cb(const gb_notify_t *notify)
{
    notify_async_t *d = malloc(sizeof(notify_async_t));
    if (!d) return;
    d->id = notify->id;
    strlcpy(d->src, notify->src, sizeof(d->src));
    strlcpy(d->title, notify->title, sizeof(d->title));
    strlcpy(d->body, notify->body, sizeof(d->body));
    lv_async_call(show_notify_async, d);
}

static void on_notify_deleted_cb(int id)
{
    lv_async_call(dismiss_notify_async, (void *)(intptr_t)id);
}

static void on_gb_time_cb(int epoch)
{
    struct timeval tv = { .tv_sec = epoch };
    settimeofday(&tv, NULL);

    time_t t = epoch;
    struct tm *tm = localtime(&t);
    rtc_pcf85063_set_time(tm);

    ESP_LOGI(TAG, "Time synced from GB: epoch=%d", epoch);
}

static void on_gb_alarm_cb(const gb_alarm_t *alarms)
{
    alarm_manager_save(alarms);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Pragmata Watch...");

    setenv("TZ", "CST-8", 1);
    tzset();

    set_time_from_build();

    nvs_flash_init();

    bsp_display_start();

    ble_manager_init();
    app_manager_init();
    rtc_pcf85063_init();
    battery_init();

    notification_ui_init();

    ble_gadgetbridge_register_notify_cb(on_notify_received_cb);
    ble_gadgetbridge_register_notify_delete_cb(on_notify_deleted_cb);
    ble_gadgetbridge_register_time_cb(on_gb_time_cb);
    ble_gadgetbridge_register_alarm_cb(on_gb_alarm_cb);

    alarm_manager_init();
    alarm_manager_start();

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
