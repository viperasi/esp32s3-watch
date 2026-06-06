#include "ble_nus.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "store/config/ble_store_config.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

/* ble_store_config_init is not declared in the public header */
extern void ble_store_config_init(void);

#define TAG "BLE_NUS"

/* NUS Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E */
#define NUS_SVC_UUID16 0xFEF5 /* Shortened for adv; full 128-bit below */

/* Full 128-bit UUIDs (little-endian) */
static const ble_uuid128_t nus_svc_uuid = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
);

static const ble_uuid128_t nus_rx_chr_uuid = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
);

static const ble_uuid128_t nus_tx_chr_uuid = BLE_UUID128_INIT(
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
);

static ble_nus_rx_cb_t s_rx_cb;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_is_connected = false;
static uint16_t s_tx_val_handle;
static uint8_t s_own_addr_type;

static int gap_event_handler(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

static int nus_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def nus_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &nus_rx_chr_uuid.u,
                .access_cb = nus_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &nus_tx_chr_uuid.u,
                .access_cb = nus_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_tx_val_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

static int nus_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t buf[512];
        uint16_t len = om_len > sizeof(buf) ? sizeof(buf) : om_len;
        os_mbuf_copydata(ctxt->om, 0, len, buf);

        ESP_LOGI(TAG, "RX %d bytes", len);
        if (s_rx_cb) {
            s_rx_cb(buf, len);
        }
    }
    return 0;
}

bool ble_nus_send(const uint8_t *data, uint16_t len)
{
    if (!s_is_connected || s_tx_val_handle == 0) {
        return false;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) {
        return false;
    }

    int rc = ble_gatts_notify_custom(s_conn_handle, s_tx_val_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "notify failed: %d", rc);
        return false;
    }
    return true;
}

bool ble_nus_is_connected(void)
{
    return s_is_connected;
}

static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields scan_fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    memset(&scan_fields, 0, sizeof(scan_fields));

    /* General discoverable + BLE only */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Include NUS 128-bit service UUID in advertising data */
    fields.uuids128 = &nus_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv set fields failed: %d", rc);
        return;
    }

    /* Put device name in scan response */
    const char *name = ble_svc_gap_device_name();
    scan_fields.name = (uint8_t *)name;
    scan_fields.name_len = strlen(name);
    scan_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&scan_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv rsp set fields failed: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x400; /* 625us * 0x400 = 640ms */
    adv_params.itvl_max = 0x800; /* 625us * 0x800 = 1280ms, ~1000ms avg */

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started");
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "CONNECT event, status=%d", event->connect.status);
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_is_connected = true;
            ESP_LOGI(TAG, "Device connected, handle=%d", s_conn_handle);

            /* Request larger MTU */
            ble_gattc_exchange_mtu(s_conn_handle, NULL, NULL);
        } else {
            s_is_connected = false;
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "DISCONNECT event, reason=%d", event->disconnect.reason);
        s_is_connected = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        start_advertising();
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update: conn=%d, mtu=%d",
                 event->mtu.conn_handle, event->mtu.value);
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "ENC_CHANGE, status=%d", event->enc_change.status);
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        {
            struct ble_gap_conn_desc desc;
            int rc2 = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            if (rc2 == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }

    default:
        break;
    }
    return 0;
}

static void ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer addr type failed: %d", rc);
        return;
    }

    uint8_t addr_val[6];
    rc = ble_hs_id_copy_addr(s_own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "copy addr failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE synced, addr: %02x:%02x:%02x:%02x:%02x:%02x",
             addr_val[5], addr_val[4], addr_val[3],
             addr_val[2], addr_val[1], addr_val[0]);

    start_advertising();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset, reason=%d", reason);
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_nus_init(ble_nus_rx_cb_t rx_callback)
{
    s_rx_cb = rx_callback;

    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble port init failed: %s", esp_err_to_name(rc));
        return;
    }

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    /* Initialize security store */
    ble_store_config_init();

    /* Just Works pairing — no PIN required, legacy mode */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ESP_LOGI(TAG, "SM config: io_cap=%d bonding=%d mitm=%d sc=%d",
             ble_hs_cfg.sm_io_cap, ble_hs_cfg.sm_bonding,
             ble_hs_cfg.sm_mitm, ble_hs_cfg.sm_sc);

    /* Set device name */
    ble_svc_gap_device_name_set("Bangle.js Pragmata");

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int err = ble_gatts_count_cfg(nus_svcs);
    if (err != 0) {
        ESP_LOGE(TAG, "gatts count cfg failed: %d", err);
        return;
    }

    err = ble_gatts_add_svcs(nus_svcs);
    if (err != 0) {
        ESP_LOGE(TAG, "gatts add svcs failed: %d", err);
        return;
    }

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "NUS service initialized");
}
