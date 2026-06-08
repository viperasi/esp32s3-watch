#include "ble_manager.h"
#include "ble_nus.h"
#include "ble_gadgetbridge.h"
#include "esp_log.h"

#define TAG "BLE_MGR"

void ble_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE manager...");
    ble_gadgetbridge_init();
    ble_nus_init(ble_gadgetbridge_on_data);
    ESP_LOGI(TAG, "BLE manager initialized");
}
