#include "rtc_pcf85063.h"
#include "pcf85063a.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

#define TAG "RTC"

static pcf85063a_dev_t rtc_dev;
static bool rtc_ready = false;

esp_err_t rtc_pcf85063_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_FAIL;
    }

    esp_err_t ret = pcf85063a_init(&rtc_dev, bus, PCF85063A_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCF85063A init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    rtc_ready = true;
    ESP_LOGI(TAG, "PCF85063A initialized");
    return ESP_OK;
}

bool rtc_pcf85063_get_time(struct tm *out)
{
    if (!rtc_ready) return false;

    pcf85063a_datetime_t dt;
    esp_err_t ret = pcf85063a_get_time_date(&rtc_dev, &dt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "RTC read failed");
        return false;
    }

    /* Sanity check: year must be >= 2024 to be considered valid */
    if (dt.year < 2024) {
        ESP_LOGI(TAG, "RTC time invalid (year=%u), ignoring", dt.year);
        return false;
    }

    out->tm_year = dt.year - 1900;
    out->tm_mon  = dt.month - 1;
    out->tm_mday = dt.day;
    out->tm_wday = dt.dotw;
    out->tm_hour = dt.hour;
    out->tm_min  = dt.min;
    out->tm_sec  = dt.sec;
    out->tm_isdst = -1;

    ESP_LOGI(TAG, "RTC time: %04u-%02u-%02u %02u:%02u:%02u",
             dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);
    return true;
}

void rtc_pcf85063_enable_minute_interrupt(void)
{
    if (!rtc_ready) return;
    uint8_t val;
    if (pcf85063a_read_register(&rtc_dev, PCF85063A_RTC_CTRL_2_ADDR, &val, 1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CTRL_2");
        return;
    }
    val |= PCF85063A_RTC_CTRL_2_MI;
    uint8_t buf[2] = {PCF85063A_RTC_CTRL_2_ADDR, val};
    if (pcf85063a_write_register(&rtc_dev, buf, 2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable minute interrupt");
    }
}

void rtc_pcf85063_disable_minute_interrupt(void)
{
    if (!rtc_ready) return;
    uint8_t val;
    if (pcf85063a_read_register(&rtc_dev, PCF85063A_RTC_CTRL_2_ADDR, &val, 1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read CTRL_2");
        return;
    }
    val &= ~PCF85063A_RTC_CTRL_2_MI;
    uint8_t buf[2] = {PCF85063A_RTC_CTRL_2_ADDR, val};
    if (pcf85063a_write_register(&rtc_dev, buf, 2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable minute interrupt");
    }
}

void rtc_pcf85063_set_time(const struct tm *t)
{
    if (!rtc_ready) return;

    pcf85063a_datetime_t dt = {
        .year  = t->tm_year + 1900,
        .month = t->tm_mon + 1,
        .day   = t->tm_mday,
        .dotw  = t->tm_wday,
        .hour  = t->tm_hour,
        .min   = t->tm_min,
        .sec   = t->tm_sec,
    };

    esp_err_t ret = pcf85063a_set_time_date(&rtc_dev, dt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTC write failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "RTC time saved: %04d-%02d-%02d %02d:%02d:%02d",
                 dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec);
    }
}
