#include "ble_gadgetbridge.h"
#include "ble_nus.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

#define TAG "GB"

/* Gadgetbridge sends messages that may arrive across multiple BLE writes.
 * Buffer incoming data and process complete lines (delimited by \n). */
#define RX_BUF_SIZE 4096
static char s_rx_buf[RX_BUF_SIZE];
static uint16_t s_rx_len = 0;

static gb_notify_cb_t s_notify_cb;
static gb_notify_delete_cb_t s_notify_delete_cb;
static gb_call_cb_t s_call_cb;
static gb_alarm_cb_t s_alarm_cb;
static gb_time_cb_t s_time_cb;
static bool s_ver_sent = false;

static void on_ble_connect(bool connected)
{
    if (!connected) {
        s_rx_len = 0;
        s_ver_sent = false;
    }
}

void ble_gadgetbridge_register_notify_cb(gb_notify_cb_t cb) { s_notify_cb = cb; }
void ble_gadgetbridge_register_notify_delete_cb(gb_notify_delete_cb_t cb) { s_notify_delete_cb = cb; }
void ble_gadgetbridge_register_call_cb(gb_call_cb_t cb) { s_call_cb = cb; }
void ble_gadgetbridge_register_alarm_cb(gb_alarm_cb_t cb) { s_alarm_cb = cb; }
void ble_gadgetbridge_register_time_cb(gb_time_cb_t cb) { s_time_cb = cb; }

void ble_gadgetbridge_init(void)
{
    s_rx_len = 0;
    ble_nus_register_connect_cb(on_ble_connect);
    ESP_LOGI(TAG, "Gadgetbridge parser initialized");
}

void ble_gadgetbridge_send(const char *json_str)
{
    uint16_t len = strlen(json_str);
    uint16_t total = len + 2; /* +2 for \r\n */
    char *buf = malloc(total);
    if (!buf) {
        ESP_LOGW(TAG, "TX alloc failed");
        return;
    }
    memcpy(buf, json_str, len);
    buf[len] = '\r';
    buf[len + 1] = '\n';
    ESP_LOGI(TAG, "TX %d bytes", total);
    bool ok = ble_nus_send((const uint8_t *)buf, total);
    free(buf);
    if (!ok) {
        ESP_LOGW(TAG, "TX failed");
    }
}

static void parse_notify(cJSON *root)
{
    gb_notify_t n = {0};
    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item) { ESP_LOGW(TAG, "notify missing id"); return; }
    n.id = id_item->valueint;

    cJSON *item;
    item = cJSON_GetObjectItem(root, "src");
    if (item && item->valuestring) strlcpy(n.src, item->valuestring, sizeof(n.src));
    item = cJSON_GetObjectItem(root, "title");
    if (item && item->valuestring) strlcpy(n.title, item->valuestring, sizeof(n.title));
    item = cJSON_GetObjectItem(root, "body");
    if (item && item->valuestring) strlcpy(n.body, item->valuestring, sizeof(n.body));
    item = cJSON_GetObjectItem(root, "sender");
    if (item && item->valuestring) strlcpy(n.sender, item->valuestring, sizeof(n.sender));
    item = cJSON_GetObjectItem(root, "tel");
    if (item && item->valuestring) strlcpy(n.tel, item->valuestring, sizeof(n.tel));

    ESP_LOGI(TAG, "notify id=%d src=%s title=%s", n.id, n.src, n.title);
    if (s_notify_cb) s_notify_cb(&n);
}

static void parse_notify_delete(cJSON *root)
{
    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item) { ESP_LOGW(TAG, "notify- missing id"); return; }
    int id = id_item->valueint;
    ESP_LOGI(TAG, "notify- id=%d", id);
    if (s_notify_delete_cb) s_notify_delete_cb(id);
}

static void parse_call(cJSON *root)
{
    gb_call_t c = {0};

    cJSON *item;
    item = cJSON_GetObjectItem(root, "cmd");
    if (item && item->valuestring) strlcpy(c.cmd, item->valuestring, sizeof(c.cmd));
    item = cJSON_GetObjectItem(root, "name");
    if (item && item->valuestring) strlcpy(c.name, item->valuestring, sizeof(c.name));
    item = cJSON_GetObjectItem(root, "number");
    if (item && item->valuestring) strlcpy(c.number, item->valuestring, sizeof(c.number));

    ESP_LOGI(TAG, "call cmd=%s name=%s number=%s", c.cmd, c.name, c.number);
    if (s_call_cb) s_call_cb(&c);
}

static void parse_alarm(cJSON *root)
{
    gb_alarm_t a = {0};
    cJSON *arr = cJSON_GetObjectItem(root, "d");
    if (!arr || !cJSON_IsArray(arr)) return;

    int count = cJSON_GetArraySize(arr);
    if (count > 10) count = 10;
    a.count = count;

    for (int i = 0; i < count; i++) {
        cJSON *entry = cJSON_GetArrayItem(arr, i);
        cJSON *h = cJSON_GetObjectItem(entry, "h");
        cJSON *m = cJSON_GetObjectItem(entry, "m");
        if (!h || !m) continue;
        a.alarms[i].hour = h->valueint;
        a.alarms[i].minute = m->valueint;
        cJSON *rep = cJSON_GetObjectItem(entry, "rep");
        if (rep) a.alarms[i].repeat = rep->valueint;
        cJSON *on = cJSON_GetObjectItem(entry, "on");
        if (on) a.alarms[i].enabled = on->valueint != 0;
    }

    for (int i = 0; i < a.count; i++) {
        ESP_LOGI(TAG, "alarm[%d] h=%d m=%d rep=%d on=%d",
                 i, a.alarms[i].hour, a.alarms[i].minute,
                 a.alarms[i].repeat, a.alarms[i].enabled);
    }
    if (s_alarm_cb) s_alarm_cb(&a);
}

static void parse_time(cJSON *root)
{
    cJSON *epoch_item = cJSON_GetObjectItem(root, "epoch");
    if (!epoch_item) { ESP_LOGW(TAG, "time missing epoch"); return; }
    int epoch = epoch_item->valueint;
    ESP_LOGI(TAG, "time epoch=%d", epoch);
    if (s_time_cb) s_time_cb(epoch);
}

static void handle_ver(void)
{
    ESP_LOGI(TAG, "ver request, replying");
    ble_gadgetbridge_send("{\"t\":\"ver\",\"fw\":\"1.0.0\",\"hw\":\"Pragmata Watch\"}");
}

/* Handle setTime(epoch) JavaScript from Gadgetbridge */
static void handle_set_time(const char *line, uint16_t len)
{
    /* Find "setTime(" and extract the number */
    const char *p = strstr(line, "setTime(");
    if (!p) return;
    p += 8; /* skip "setTime(" */
    char *end;
    long epoch = strtol(p, &end, 10);
    if (end != p) {
        ESP_LOGI(TAG, "setTime epoch=%ld", epoch);
        if (s_time_cb) s_time_cb((int)epoch);
    }
}

static void process_line(const char *line, uint16_t line_len)
{
    if (line_len == 0) return;

    ESP_LOGI(TAG, "process_line %d: %.*s", line_len, line_len > 200 ? 200 : line_len, line);

    /* Skip \x10 prefix if present */
    const char *p = line;
    uint16_t len = line_len;
    if ((uint8_t)p[0] == 0x10) {
        p++;
        len--;
    }
    if (len == 0) return;

    /* Strip trailing \r */
    if (len > 0 && p[len - 1] == '\r') {
        len--;
    }

    /* Handle setTime() JavaScript from Gadgetbridge */
    if (strncmp(p, "setTime(", 8) == 0) {
        handle_set_time(p, len);
        return;
    }

    /* Check GB( prefix and ) suffix */
    if (len < 4 || strncmp(p, "GB(", 3) != 0 || p[len - 1] != ')') {
        ESP_LOGD(TAG, "not a GB() message: %.*s", len > 100 ? 100 : len, p);
        return;
    }

    /* Extract JSON: skip "GB(" and drop ")" */
    const char *json_start = p + 3;
    uint16_t json_len = len - 4;

    char json_buf[2048];
    if (json_len >= sizeof(json_buf)) {
        ESP_LOGW(TAG, "JSON too long: %d", json_len);
        return;
    }
    memcpy(json_buf, json_start, json_len);
    json_buf[json_len] = '\0';

    cJSON *root = cJSON_Parse(json_buf);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed: %s", json_buf);
        return;
    }

    cJSON *t = cJSON_GetObjectItem(root, "t");
    if (!t || !t->valuestring) {
        cJSON_Delete(root);
        return;
    }

    const char *type = t->valuestring;
    if (strcmp(type, "notify") == 0) {
        parse_notify(root);
    } else if (strcmp(type, "notify-") == 0) {
        parse_notify_delete(root);
    } else if (strcmp(type, "call") == 0) {
        parse_call(root);
    } else if (strcmp(type, "alarm") == 0) {
        parse_alarm(root);
    } else if (strcmp(type, "time") == 0) {
        parse_time(root);
    } else if (strcmp(type, "ver") == 0) {
        handle_ver();
    } else if (strcmp(type, "is_gps_active") == 0) {
        ESP_LOGI(TAG, "is_gps_active request, replying");
        ble_gadgetbridge_send("{\"t\":\"gps_power\",\"status\":false}");
    } else {
        ESP_LOGD(TAG, "unknown GB type: %s", type);
    }

    cJSON_Delete(root);
}

void ble_gadgetbridge_on_data(const uint8_t *data, uint16_t len)
{
    /* Send version info on first data from Gadgetbridge (safe context) */
    if (!s_ver_sent) {
        s_ver_sent = true;
        ESP_LOGI(TAG, "First data, sending version info");
        ble_gadgetbridge_send("{\"t\":\"ver\",\"fw\":\"1.0.0\",\"hw\":\"Pragmata Watch\"}");
    }

    ESP_LOGI(TAG, "on_data %d bytes (buf %d)", len, s_rx_len);

    if (s_rx_len + len >= sizeof(s_rx_buf)) {
        ESP_LOGW(TAG, "RX buffer overflow, resetting");
        s_rx_len = 0;
    }

    memcpy(s_rx_buf + s_rx_len, data, len);
    s_rx_len += len;
    s_rx_buf[s_rx_len] = '\0';

    /* Process complete lines delimited by \n */
    char *line_start = s_rx_buf;
    char *nl;
    while ((nl = strchr(line_start, '\n')) != NULL) {
        *nl = '\0';
        uint16_t line_len = nl - line_start;
        process_line(line_start, line_len);
        line_start = nl + 1;
    }

    /* Move remaining partial data to front of buffer */
    uint16_t remaining = s_rx_len - (line_start - s_rx_buf);
    if (remaining > 0 && line_start != s_rx_buf) {
        memmove(s_rx_buf, line_start, remaining);
    }
    s_rx_len = remaining;
    s_rx_buf[s_rx_len] = '\0';
}
