#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef void (*ble_nus_rx_cb_t)(const uint8_t *data, uint16_t len);
typedef void (*ble_nus_connect_cb_t)(bool connected);

void ble_nus_init(ble_nus_rx_cb_t rx_callback);
bool ble_nus_send(const uint8_t *data, uint16_t len);
bool ble_nus_is_connected(void);
void ble_nus_register_connect_cb(ble_nus_connect_cb_t cb);
