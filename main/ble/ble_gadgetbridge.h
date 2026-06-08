#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int id;
    char src[32];
    char title[64];
    char body[256];
    char sender[32];
    char tel[20];
} gb_notify_t;

typedef struct {
    char cmd[16];
    char name[64];
    char number[20];
} gb_call_t;

typedef struct {
    int hour;
    int minute;
    int repeat;
    bool enabled;
} gb_alarm_entry_t;

typedef struct {
    gb_alarm_entry_t alarms[10];
    int count;
} gb_alarm_t;

typedef void (*gb_notify_cb_t)(const gb_notify_t *notify);
typedef void (*gb_notify_delete_cb_t)(int id);
typedef void (*gb_call_cb_t)(const gb_call_t *call);
typedef void (*gb_alarm_cb_t)(const gb_alarm_t *alarm);
typedef void (*gb_time_cb_t)(int epoch);

void ble_gadgetbridge_init(void);
void ble_gadgetbridge_register_notify_cb(gb_notify_cb_t cb);
void ble_gadgetbridge_register_notify_delete_cb(gb_notify_delete_cb_t cb);
void ble_gadgetbridge_register_call_cb(gb_call_cb_t cb);
void ble_gadgetbridge_register_alarm_cb(gb_alarm_cb_t cb);
void ble_gadgetbridge_register_time_cb(gb_time_cb_t cb);

void ble_gadgetbridge_on_data(const uint8_t *data, uint16_t len);
void ble_gadgetbridge_send(const char *json_str);
