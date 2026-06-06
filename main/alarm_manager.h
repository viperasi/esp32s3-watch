#pragma once

#include "ble/ble_gadgetbridge.h"

void alarm_manager_init(void);
void alarm_manager_save(const gb_alarm_t *alarms);
int alarm_manager_get_count(void);
const gb_alarm_entry_t *alarm_manager_get(int index);
void alarm_manager_check(void);
void alarm_manager_start(void);
