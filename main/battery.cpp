#include "battery.h"
#include "bsp/esp-bsp.h"
#include "XPowersAXP2101.hpp"
#include "esp_log.h"

#define TAG "BATTERY"

static XPowersAXP2101 pmic;
static bool battery_ready = false;

esp_err_t battery_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_FAIL;
    }

    if (!pmic.begin(bus, AXP2101_SLAVE_ADDRESS)) {
        ESP_LOGE(TAG, "AXP2101 init failed");
        return ESP_FAIL;
    }

    battery_ready = true;
    ESP_LOGI(TAG, "AXP2101 initialized");
    return ESP_OK;
}

int battery_get_percent(void)
{
    if (!battery_ready) return -1;
    return pmic.getBatteryPercent();
}
