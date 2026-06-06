#include "alarm_manager.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

#define TAG "ALARM_MGR"

#define NVS_NAMESPACE "pragmata"
#define NVS_KEY_ALARMS "gb_alarms"

static gb_alarm_t s_alarms;
static bool s_alarm_fired[10];

LV_FONT_DECLARE(font_barlow_38)

static lv_obj_t *alarm_card = NULL;
static lv_timer_t *alarm_auto_timer = NULL;

static void close_alarm_card(void)
{
    if (alarm_auto_timer) {
        lv_timer_del(alarm_auto_timer);
        alarm_auto_timer = NULL;
    }
    if (alarm_card) {
        lv_obj_del(alarm_card);
        alarm_card = NULL;
    }
}

static void alarm_card_click_cb(lv_event_t *e)
{
    (void)e;
    close_alarm_card();
}

static void alarm_auto_timer_cb(lv_timer_t *timer)
{
    close_alarm_card();
}

static void show_alarm_ui_async(void *user_data)
{
    char *text = (char *)user_data;

    if (alarm_card) close_alarm_card();

    lv_obj_t *scr = lv_screen_active();
    alarm_card = lv_obj_create(scr);
    lv_obj_remove_style_all(alarm_card);
    lv_obj_set_size(alarm_card, 380, 120);
    lv_obj_align(alarm_card, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(alarm_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(alarm_card, LV_OPA_80, 0);
    lv_obj_set_style_radius(alarm_card, 16, 0);
    lv_obj_set_style_pad_all(alarm_card, 16, 0);
    lv_obj_set_style_border_width(alarm_card, 0, 0);
    lv_obj_clear_flag(alarm_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(alarm_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(alarm_card, alarm_card_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(alarm_card);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &font_barlow_38, 0);
    lv_label_set_text(lbl, text);

    alarm_auto_timer = lv_timer_create(alarm_auto_timer_cb, 30000, NULL);
    lv_timer_set_repeat_count(alarm_auto_timer, 1);

    free(text);
}

void alarm_manager_init(void)
{
    memset(&s_alarms, 0, sizeof(s_alarms));
    memset(s_alarm_fired, 0, sizeof(s_alarm_fired));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved alarms");
        return;
    }

    size_t len = 0;
    err = nvs_get_str(handle, NVS_KEY_ALARMS, NULL, &len);
    if (err != ESP_OK || len == 0) {
        nvs_close(handle);
        ESP_LOGI(TAG, "No saved alarms in NVS");
        return;
    }

    char *json_str = malloc(len);
    if (!json_str) {
        nvs_close(handle);
        return;
    }

    err = nvs_get_str(handle, NVS_KEY_ALARMS, json_str, &len);
    nvs_close(handle);
    if (err != ESP_OK) {
        free(json_str);
        return;
    }

    /* Parse JSON array: [{"h":7,"m":30,"rep":127,"on":1},...] */
    cJSON *arr = cJSON_Parse(json_str);
    free(json_str);
    if (!arr || !cJSON_IsArray(arr)) {
        if (arr) cJSON_Delete(arr);
        return;
    }

    int count = cJSON_GetArraySize(arr);
    if (count > 10) count = 10;
    s_alarms.count = count;

    for (int i = 0; i < count; i++) {
        cJSON *entry = cJSON_GetArrayItem(arr, i);
        s_alarms.alarms[i].hour = cJSON_GetObjectItem(entry, "h")->valueint;
        s_alarms.alarms[i].minute = cJSON_GetObjectItem(entry, "m")->valueint;
        cJSON *rep = cJSON_GetObjectItem(entry, "rep");
        if (rep) s_alarms.alarms[i].repeat = rep->valueint;
        cJSON *on = cJSON_GetObjectItem(entry, "on");
        if (on) s_alarms.alarms[i].enabled = on->valueint != 0;
    }

    cJSON_Delete(arr);
    ESP_LOGI(TAG, "Loaded %d alarms from NVS", s_alarms.count);
}

void alarm_manager_save(const gb_alarm_t *alarms)
{
    s_alarms = *alarms;
    memset(s_alarm_fired, 0, sizeof(s_alarm_fired));

    /* Build JSON array string */
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < alarms->count; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "h", alarms->alarms[i].hour);
        cJSON_AddNumberToObject(entry, "m", alarms->alarms[i].minute);
        cJSON_AddNumberToObject(entry, "rep", alarms->alarms[i].repeat);
        cJSON_AddNumberToObject(entry, "on", alarms->alarms[i].enabled ? 1 : 0);
        cJSON_AddItemToArray(arr, entry);
    }

    char *json_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!json_str) return;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        free(json_str);
        ESP_LOGE(TAG, "Failed to open NVS for write");
        return;
    }

    err = nvs_set_str(handle, NVS_KEY_ALARMS, json_str);
    if (err == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Saved %d alarms to NVS", alarms->count);
    } else {
        ESP_LOGE(TAG, "Failed to save alarms: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    free(json_str);
}

int alarm_manager_get_count(void)
{
    return s_alarms.count;
}

const gb_alarm_entry_t *alarm_manager_get(int index)
{
    if (index < 0 || index >= s_alarms.count) return NULL;
    return &s_alarms.alarms[index];
}

void alarm_manager_check(void)
{
    time_t now;
    struct tm tm_now;
    time(&now);
    localtime_r(&now, &tm_now);

    for (int i = 0; i < s_alarms.count; i++) {
        if (!s_alarms.alarms[i].enabled) continue;
        if (s_alarm_fired[i]) continue;

        if (tm_now.tm_hour == s_alarms.alarms[i].hour &&
            tm_now.tm_min == s_alarms.alarms[i].minute) {

            s_alarm_fired[i] = true;

            char text[32];
            snprintf(text, sizeof(text), "闹钟 %02d:%02d",
                     s_alarms.alarms[i].hour, s_alarms.alarms[i].minute);
            ESP_LOGI(TAG, "Alarm triggered: %s", text);

            char *buf = strdup(text);
            if (buf) lv_async_call(show_alarm_ui_async, buf);
        }
    }

    /* Reset fired flags when minute passes */
    for (int i = 0; i < s_alarms.count; i++) {
        if (s_alarm_fired[i] &&
            (tm_now.tm_hour != s_alarms.alarms[i].hour ||
             tm_now.tm_min != s_alarms.alarms[i].minute)) {
            s_alarm_fired[i] = false;
        }
    }
}

static void alarm_check_task(void *arg)
{
    for (;;) {
        alarm_manager_check();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void alarm_manager_start(void)
{
    xTaskCreate(alarm_check_task, "alarm_chk", 3072, NULL, 1, NULL);
    ESP_LOGI(TAG, "Alarm checker started");
}
