#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t battery_init(void);
int battery_get_percent(void);

#ifdef __cplusplus
}
#endif
