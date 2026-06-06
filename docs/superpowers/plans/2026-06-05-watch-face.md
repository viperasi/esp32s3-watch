# Pragmata Watch Face Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 ESP32-S3 Touch AMOLED 2.06" 开发板上实现 NASA 蓝图风格的 Pragmata 表盘界面。

**Architecture:** 使用 Waveshare BSP 初始化硬件 + LVGL 9 渲染 UI。`main.c` 负责硬件初始化和 LVGL 启动，`watch_face.c` 封装表盘 UI 的创建和更新逻辑。LVGL timer 每秒驱动时间刷新。

**Tech Stack:** ESP-IDF v5.4.2, LVGL 9.3+, waveshare/esp32_s3_touch_amoled_2_06 BSP

---

## File Structure

```
test_watch/
├── CMakeLists.txt                  # 项目入口，引入 ESP-IDF 构建系统
├── sdkconfig.defaults              # SDK 默认配置（Flash/PSRAM/LVGL/屏幕）
├── partitions.csv                  # 分区表：8M app + 7M spiffs
├── main/
│   ├── CMakeLists.txt              # 注册 main 组件源文件
│   ├── idf_component.yml           # 依赖：BSP + LVGL 9
│   ├── main.c                      # 入口：BSP初始化 → LVGL启动 → 创建表盘
│   ├── watch_face.h                # 表盘公开接口
│   └── watch_face.c                # 表盘 UI 实现（布局+定时更新）
```

---

### Task 1: 项目脚手架

**Files:**
- Create: `CMakeLists.txt`
- Create: `partitions.csv`
- Create: `sdkconfig.defaults`
- Create: `main/CMakeLists.txt`
- Create: `main/idf_component.yml`
- Create: `main/main.c` (最小 Hello World)

- [ ] **Step 1: 创建项目根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pragmata_watch)
```

- [ ] **Step 2: 创建分区表 partitions.csv**

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, ,        8M,
storage,  data, spiffs,  ,        7M,
```

- [ ] **Step 3: 创建 sdkconfig.defaults**

从 test_lvgl 项目复制并调整，启用 LVGL 大字体支持：

```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y
CONFIG_FREERTOS_HZ=1000
CONFIG_LV_USE_CLIB_MALLOC=y
CONFIG_LV_USE_CLIB_STRING=y
CONFIG_LV_USE_CLIB_SPRINTF=y
CONFIG_LV_DEF_REFR_PERIOD=15
CONFIG_LV_OS_FREERTOS=y
CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2
CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y
CONFIG_LV_FONT_MONTSERRAT_14=y
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_20=y
CONFIG_LV_FONT_MONTSERRAT_24=y
CONFIG_LV_FONT_MONTSERRAT_48=y
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
```

- [ ] **Step 4: 创建 main/CMakeLists.txt**

```cmake
idf_component_register(SRCS "main.c" "watch_face.c" INCLUDE_DIRS ".")
```

- [ ] **Step 5: 创建 main/idf_component.yml**

```yaml
dependencies:
  waveshare/esp32_s3_touch_amoled_2_06:
    version: "*"
  lvgl/lvgl:
    version: ">=9.3.0"
    public: true
```

- [ ] **Step 6: 创建 main/main.c 最小入口**

验证 BSP + LVGL 能启动，显示 "Hello World"：

```c
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#define TAG "WATCH"

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Pragmata Watch...");

    bsp_display_start();

    bsp_display_lock(0);
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "PRAGMATA WATCH v0.1");
    lv_obj_center(label);
    bsp_display_unlock();

    ESP_LOGI(TAG, "Display ready");
}
```

- [ ] **Step 7: 创建空的 watch_face.h 和 watch_face.c**

`main/watch_face.h`:
```c
#pragma once

void watch_face_create(void);
```

`main/watch_face.c`:
```c
#include "watch_face.h"

void watch_face_create(void)
{
}
```

- [ ] **Step 8: 编译验证**

```bash
cd /Users/xumin/Documents/workspace/esp32/test_watch
idf.py build
```

Expected: 编译成功，无错误。

- [ ] **Step 9: 烧录验证**

```bash
idf.py -p /dev/tty.usbmodem* flash monitor
```

Expected: 屏幕显示 "PRAGMATA WATCH v0.1" 文字。

- [ ] **Step 10: 提交**

```bash
git init
git add CMakeLists.txt partitions.csv sdkconfig.defaults main/
git commit -m "feat: project scaffold with BSP + LVGL hello world"
```

---

### Task 2: 表盘静态布局

**Files:**
- Modify: `main/main.c` — 调用 `watch_face_create()` 替换 Hello World
- Modify: `main/watch_face.h` — 添加数据结构声明
- Modify: `main/watch_face.c` — 实现完整静态布局

- [ ] **Step 1: 更新 watch_face.h，定义表盘数据结构**

```c
#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *hours_label;
    lv_obj_t *minutes_label;
    lv_obj_t *date_year_label;
    lv_obj_t *date_md_label;
    lv_obj_t *day_label;
    lv_obj_t *seconds_label;
    lv_obj_t *bat_percent_label;
    lv_obj_t *bat_bar;
    lv_timer_t *update_timer;
} watch_face_t;

void watch_face_create(void);
```

- [ ] **Step 2: 更新 main.c 调用表盘**

```c
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "watch_face.h"

#define TAG "WATCH"

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Pragmata Watch...");

    bsp_display_start();

    bsp_display_lock(0);
    watch_face_create();
    bsp_display_unlock();

    ESP_LOGI(TAG, "Watch face ready");
}
```

- [ ] **Step 3: 实现 watch_face.c 静态布局**

使用 LVGL 内置 Montserrat 字体先跑通布局，后续 Task 替换为自定义字体。

布局逻辑（410x502 屏幕）：
- 顶部栏 (top bar): PNL-042 + REV 标签 + 红白蓝分割线
- 上半区 (hours row): 左侧小时 + 右侧日期（年+月日）
- 分割线
- 下半区 (minutes row): 左侧星期 + 右侧分钟
- 分割线
- 底部栏 (bottom row): 秒数 + 电量条 + 状态灯

```c
#include "watch_face.h"
#include "esp_log.h"
#include <time.h>

#define TAG "WATCH_FACE"

/* Colors (RGB565) */
#define COLOR_BG        lv_color_hex(0xE8E8E4)
#define COLOR_TEXT      lv_color_hex(0x1A1F2E)
#define COLOR_TEXT2     lv_color_hex(0x555555)
#define COLOR_TEXT3     lv_color_hex(0x888888)
#define COLOR_RED       lv_color_hex(0xC0504D)
#define COLOR_BLUE      lv_color_hex(0x4472C4)
#define COLOR_GRAY      lv_color_hex(0xCCCCCC)
#define COLOR_GREEN     lv_color_hex(0x4CAF50)

static watch_face_t wf;

static lv_obj_t* create_stripe(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 374, 3);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, 2, 0);

    lv_obj_t *r = lv_obj_create(cont);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, 90, 3);
    lv_obj_set_style_bg_color(r, COLOR_RED, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);

    lv_obj_t *w = lv_obj_create(cont);
    lv_obj_remove_style_all(w);
    lv_obj_set_flex_grow(w, 1);
    lv_obj_set_style_bg_color(w, COLOR_GRAY, 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);

    lv_obj_t *b = lv_obj_create(cont);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 110, 3);
    lv_obj_set_style_bg_color(b, COLOR_BLUE, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);

    return cont;
}

static lv_obj_t* create_crosshair(lv_obj_t *parent, lv_point_t pos) {
    lv_obj_t *h = lv_obj_create(parent);
    lv_obj_remove_style_all(h);
    lv_obj_set_size(h, 12, 1);
    lv_obj_set_style_bg_color(h, COLOR_TEXT3, 0);
    lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
    lv_obj_align(h, LV_ALIGN_TOP_LEFT, pos.x - 6, pos.y);

    lv_obj_t *v = lv_obj_create(parent);
    lv_obj_remove_style_all(v);
    lv_obj_set_size(v, 1, 12);
    lv_obj_set_style_bg_color(v, COLOR_TEXT3, 0);
    lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, pos.x, pos.y - 6);

    return h;
}

void watch_face_create(void)
{
    wf.screen = lv_screen_active();
    lv_obj_set_style_bg_color(wf.screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(wf.screen, LV_OPA_COVER, 0);

    /* Grid background: draw horizontal and vertical lines */
    for (int i = 0; i < 502; i += 20) {
        lv_obj_t *line = lv_obj_create(wf.screen);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, 410, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(line, 15, 0);
        lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, i);
    }
    for (int x = 0; x < 410; x += 20) {
        lv_obj_t *line = lv_obj_create(wf.screen);
        lv_obj_remove_style_all(line);
        lv_obj_set_size(line, 1, 502);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(line, 15, 0);
        lv_obj_align(line, LV_ALIGN_TOP_LEFT, x, 0);
    }

    /* Crosshairs at corners */
    create_crosshair(wf.screen, (lv_point_t){20, 18});
    create_crosshair(wf.screen, (lv_point_t){390, 18});
    create_crosshair(wf.screen, (lv_point_t){20, 478});
    create_crosshair(wf.screen, (lv_point_t){390, 478});

    /* ===== TOP BAR ===== */
    lv_obj_t *top_bar = lv_obj_create(wf.screen);
    lv_obj_remove_style_all(top_bar);
    lv_obj_set_size(top_bar, 410, 24);
    lv_obj_align(top_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(top_bar, 18, 0);

    lv_obj_t *pnl = lv_label_create(top_bar);
    lv_label_set_text(pnl, "PNL-042 \xC2\xB7 WATCH");
    lv_obj_set_style_text_color(pnl, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(pnl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(pnl, 3, 0);

    lv_obj_t *rev = lv_label_create(top_bar);
    lv_label_set_text(rev, "REV.2026.06");
    lv_obj_set_style_text_color(rev, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(rev, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(rev, 2, 0);

    /* Top stripe */
    lv_obj_t *stripe1 = create_stripe(wf.screen);
    lv_obj_align(stripe1, LV_ALIGN_TOP_LEFT, 18, 28);

    /* ===== HOURS ROW ===== */
    /* Hours - left side, large */
    lv_obj_t *hrs_label = lv_label_create(wf.screen);
    lv_label_set_text(hrs_label, "HOURS");
    lv_obj_set_style_text_color(hrs_label, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(hrs_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(hrs_label, 3, 0);
    lv_obj_align(hrs_label, LV_ALIGN_TOP_LEFT, 18, 40);

    wf.hours_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.hours_label, "21");
    lv_obj_set_style_text_color(wf.hours_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(wf.hours_label, &lv_font_montserrat_48, 0);
    lv_obj_align(wf.hours_label, LV_ALIGN_TOP_LEFT, 10, 52);

    /* Date - right side of hours row */
    lv_obj_t *date_lbl = lv_label_create(wf.screen);
    lv_label_set_text(date_lbl, "DATE");
    lv_obj_set_style_text_color(date_lbl, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(date_lbl, 3, 0);
    lv_obj_align(date_lbl, LV_ALIGN_TOP_RIGHT, -18, 40);

    wf.date_year_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.date_year_label, "2026");
    lv_obj_set_style_text_color(wf.date_year_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(wf.date_year_label, &lv_font_montserrat_24, 0);
    lv_obj_align(wf.date_year_label, LV_ALIGN_TOP_RIGHT, -18, 54);

    wf.date_md_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.date_md_label, "06.05");
    lv_obj_set_style_text_color(wf.date_md_label, COLOR_TEXT2, 0);
    lv_obj_set_style_text_font(wf.date_md_label, &lv_font_montserrat_24, 0);
    lv_obj_align(wf.date_md_label, LV_ALIGN_TOP_RIGHT, -18, 78);

    /* ===== MIDDLE DIVIDER ===== */
    lv_obj_t *mid_div = lv_obj_create(wf.screen);
    lv_obj_remove_style_all(mid_div);
    lv_obj_set_size(mid_div, 374, 1);
    lv_obj_set_style_bg_color(mid_div, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mid_div, 31, 0);
    lv_obj_align(mid_div, LV_ALIGN_CENTER, 0, -30);

    /* ===== MINUTES ROW ===== */
    /* Day - left side */
    lv_obj_t *day_lbl = lv_label_create(wf.screen);
    lv_label_set_text(day_lbl, "DAY");
    lv_obj_set_style_text_color(day_lbl, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(day_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(day_lbl, 3, 0);
    lv_obj_align(day_lbl, LV_ALIGN_TOP_LEFT, 18, 230);

    wf.day_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.day_label, "FRI");
    lv_obj_set_style_text_color(wf.day_label, COLOR_BLUE, 0);
    lv_obj_set_style_text_font(wf.day_label, &lv_font_montserrat_48, 0);
    lv_obj_align(wf.day_label, LV_ALIGN_TOP_LEFT, 18, 242);

    /* Minutes - right side, large */
    lv_obj_t *min_label = lv_label_create(wf.screen);
    lv_label_set_text(min_label, "MINUTES");
    lv_obj_set_style_text_color(min_label, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(min_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(min_label, 3, 0);
    lv_obj_align(min_label, LV_ALIGN_TOP_RIGHT, -18, 230);

    wf.minutes_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.minutes_label, "32");
    lv_obj_set_style_text_color(wf.minutes_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(wf.minutes_label, &lv_font_montserrat_48, 0);
    lv_obj_align(wf.minutes_label, LV_ALIGN_TOP_RIGHT, -10, 252);

    /* ===== BOTTOM DIVIDER ===== */
    lv_obj_t *bot_div = lv_obj_create(wf.screen);
    lv_obj_remove_style_all(bot_div);
    lv_obj_set_size(bot_div, 374, 1);
    lv_obj_set_style_bg_color(bot_div, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(bot_div, 31, 0);
    lv_obj_align(bot_div, LV_ALIGN_TOP_LEFT, 18, 400);

    /* ===== BOTTOM ROW (SEC + Battery) ===== */
    /* Seconds */
    lv_obj_t *sec_lbl = lv_label_create(wf.screen);
    lv_label_set_text(sec_lbl, "SEC");
    lv_obj_set_style_text_color(sec_lbl, COLOR_TEXT3, 0);
    lv_obj_set_style_text_font(sec_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(sec_lbl, 3, 0);
    lv_obj_align(sec_lbl, LV_ALIGN_TOP_LEFT, 18, 410);

    wf.seconds_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.seconds_label, "25");
    lv_obj_set_style_text_color(wf.seconds_label, COLOR_RED, 0);
    lv_obj_set_style_text_font(wf.seconds_label, &lv_font_montserrat_24, 0);
    lv_obj_align(wf.seconds_label, LV_ALIGN_TOP_LEFT, 18, 422);

    /* Battery */
    lv_obj_t *bat_lbl = lv_label_create(wf.screen);
    lv_label_set_text(bat_lbl, "PWR SYS");
    lv_obj_set_style_text_color(bat_lbl, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(bat_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(bat_lbl, 3, 0);
    lv_obj_align(bat_lbl, LV_ALIGN_TOP_RIGHT, -90, 410);

    wf.bat_percent_label = lv_label_create(wf.screen);
    lv_label_set_text(wf.bat_percent_label, "80%");
    lv_obj_set_style_text_color(wf.bat_percent_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(wf.bat_percent_label, &lv_font_montserrat_10, 0);
    lv_obj_align(wf.bat_percent_label, LV_ALIGN_TOP_RIGHT, -18, 410);

    wf.bat_bar = lv_bar_create(wf.screen);
    lv_obj_set_size(wf.bat_bar, 150, 7);
    lv_obj_align(wf.bat_bar, LV_ALIGN_TOP_RIGHT, -18, 426);
    lv_bar_set_range(wf.bat_bar, 0, 100);
    lv_bar_set_value(wf.bat_bar, 80, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(wf.bat_bar, lv_color_hex(0xD0D0CC), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wf.bat_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(wf.bat_bar, COLOR_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(wf.bat_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    /* ===== BOTTOM STRIPE + STATUS ===== */
    lv_obj_t *stripe2 = create_stripe(wf.screen);
    lv_obj_align(stripe2, LV_ALIGN_TOP_LEFT, 18, 448);

    lv_obj_t *status_bar = lv_obj_create(wf.screen);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, 410, 20);
    lv_obj_align(status_bar, LV_ALIGN_TOP_LEFT, 0, 458);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(status_bar, 18, 0);

    const char *status_names[] = {"SYS", "NAV", "COM", "MEM 64K"};
    const lv_color_t status_colors[] = {COLOR_GREEN, COLOR_BLUE, COLOR_RED, COLOR_TEXT3};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *item = lv_obj_create(status_bar);
        lv_obj_remove_style_all(item);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(item, 4, 0);

        if (i < 3) {
            lv_obj_t *dot = lv_obj_create(item);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 6, 6);
            lv_obj_set_style_bg_color(dot, status_colors[i], 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(dot, 3, 0);
        }

        lv_obj_t *txt = lv_label_create(item);
        lv_label_set_text(txt, status_names[i]);
        lv_obj_set_style_text_color(txt, lv_color_hex(0x555555), 0);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_letter_space(txt, 2, 0);
    }

    ESP_LOGI(TAG, "Watch face created");
}
```

- [ ] **Step 4: 编译验证**

```bash
idf.py build
```

Expected: 编译成功。

- [ ] **Step 5: 烧录验证**

```bash
idf.py -p /dev/tty.usbmodem* flash monitor
```

Expected: 屏幕显示完整的 Pragmata 风格表盘静态布局（使用内置字体，字号偏小但结构正确）。

- [ ] **Step 6: 提交**

```bash
git add main/
git commit -m "feat: static watch face layout with LVGL built-in fonts"
```

---

### Task 3: 时间动态更新

**Files:**
- Modify: `main/watch_face.c` — 添加 LVGL timer，每秒更新时/分/秒/日期/星期

- [ ] **Step 1: 在 watch_face.c 中添加时间更新函数和定时器**

在 `watch_face_create()` 函数末尾，`ESP_LOGI` 之前，添加定时器创建：

```c
static void watch_face_update_cb(lv_timer_t *timer)
{
    time_t now;
    time(&now);
    struct tm *t = localtime(&now);

    char buf[16];

    snprintf(buf, sizeof(buf), "%02d", t->tm_hour);
    lv_label_set_text(wf.hours_label, buf);

    snprintf(buf, sizeof(buf), "%02d", t->tm_min);
    lv_label_set_text(wf.minutes_label, buf);

    snprintf(buf, sizeof(buf), "%02d", t->tm_sec);
    lv_label_set_text(wf.seconds_label, buf);

    snprintf(buf, sizeof(buf), "%d", t->tm_year + 1900);
    lv_label_set_text(wf.date_year_label, buf);

    snprintf(buf, sizeof(buf), "%02d.%02d", t->tm_mon + 1, t->tm_mday);
    lv_label_set_text(wf.date_md_label, buf);

    const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    lv_label_set_text(wf.day_label, days[t->tm_wday]);
}
```

在 `watch_face_create()` 的 `ESP_LOGI` 之前添加：

```c
    /* Set initial time from RTC/system */
    watch_face_update_cb(NULL);

    /* Create 1-second update timer */
    wf.update_timer = lv_timer_create(watch_face_update_cb, 1000, NULL);
```

- [ ] **Step 2: 在 main.c 中设置时区并同步 RTC**

在 `bsp_display_start()` 之前添加：

```c
#include <sys/time.h>

    /* Set timezone to UTC+8 */
    setenv("TZ", "CST-8", 1);
    tzset();
```

注意：ESP32 默认从编译时间获取初始时间。如果板载 PCF85063 RTC 已由 BSP 初始化，系统时间会更准确。

- [ ] **Step 3: 编译验证**

```bash
idf.py build
```

Expected: 编译成功。

- [ ] **Step 4: 烧录验证**

```bash
idf.py -p /dev/tty.usbmodem* flash monitor
```

Expected: 秒数每秒跳动更新，分钟在整分钟时更新，日期和星期正确。

- [ ] **Step 5: 提交**

```bash
git add main/
git commit -m "feat: real-time clock update with LVGL timer"
```

---

### Task 4: 自定义字体集成

**Files:**
- Modify: `main/watch_face.c` — 替换内置字体为自定义大号字体
- Create: `main/fonts/` 目录，存放转换后的 LVGL 字体文件

LVGL 内置字体最大 48px，设计需要 172px 小时/分钟、72px 星期、40px 秒数。需要用 `lv_font_conv` 工具从 TTF 转换。

- [ ] **Step 1: 准备字体文件**

下载 Space Mono 和 Barlow Condensed 的 TTF 文件，放入 `main/fonts/` 目录：
- `main/fonts/SpaceMono-Bold.ttf`
- `main/fonts/BarlowCondensed-SemiBold.ttf`
- `main/fonts/BarlowCondensed-Medium.ttf`

- [ ] **Step 2: 安装 lv_font_conv 工具**

```bash
npm install -g lv_font_conv
```

- [ ] **Step 3: 转换字体为 LVGL C 文件**

生成所需字号（仅包含数字和大写字母以节省 Flash 空间）：

```bash
cd /Users/xumin/Documents/workspace/esp32/test_watch/main/fonts

# 172px for hours and minutes - numbers only
lv_font_conv --font BarlowCondensed-SemiBold.ttf -r 0x30-0x39 -r 0x3A --size 172 --bpp 4 --no-compress -o font_barlow_172.c

# 72px for day name - uppercase letters
lv_font_conv --font BarlowCondensed-SemiBold.ttf -r 0x41-0x5A --size 72 --bpp 4 --no-compress -o font_barlow_72.c

# 38px for date
lv_font_conv --font BarlowCondensed-Medium.ttf -r 0x30-0x39 -r 0x2E --size 38 --bpp 4 --no-compress -o font_barlow_38.c

# 40px for seconds - numbers only
lv_font_conv --font SpaceMono-Bold.ttf -r 0x30-0x39 --size 40 --bpp 4 --no-compress -o font_spacemono_40.c

# 8-10px for labels
lv_font_conv --font SpaceMono-Bold.ttf -r 0x20-0x7E --size 10 --bpp 4 --no-compress -o font_spacemono_10.c
```

- [ ] **Step 4: 更新 main/CMakeLists.txt 包含字体文件**

```cmake
idf_component_register(SRCS "main.c" "watch_face.c" INCLUDE_DIRS "." fonts)
```

- [ ] **Step 5: 在 watch_face.c 中声明外部字体并替换**

在文件顶部添加：

```c
LV_FONT_DECLARE(font_barlow_172)
LV_FONT_DECLARE(font_barlow_72)
LV_FONT_DECLARE(font_barlow_38)
LV_FONT_DECLARE(font_spacemono_40)
LV_FONT_DECLARE(font_spacemono_10)
```

替换所有 `lv_font_montserrat_*` 引用：
- `&lv_font_montserrat_48` (hours/minutes) → `&font_barlow_172`
- `&lv_font_montserrat_48` (day) → `&font_barlow_72`
- `&lv_font_montserrat_24` (date) → `&font_barlow_38`
- `&lv_font_montserrat_24` (seconds) → `&font_spacemono_40`
- `&lv_font_montserrat_10` (labels) → `&font_spacemono_10`

- [ ] **Step 6: 调整布局坐标**

因为字号从 48px 变为 172px，需要调整各元素的 Y 坐标和对齐方式，使布局与设计稿一致。具体数值需要烧录后目测微调。

- [ ] **Step 7: 编译验证**

```bash
idf.py build
```

Expected: 编译成功。注意 Flash 使用量会增加。

- [ ] **Step 8: 烧录验证**

```bash
idf.py -p /dev/tty.usbmodem* flash monitor
```

Expected: 大号字体显示，172px 的小时和分钟占据主要屏幕空间，视觉效果接近设计稿。

- [ ] **Step 9: 提交**

```bash
git add main/
git commit -m "feat: custom Barlow Condensed + Space Mono fonts for watch face"
```

---

### Task 5: 布局微调与最终验证

**Files:**
- Modify: `main/watch_face.c` — 微调对齐、间距、颜色

- [ ] **Step 1: 根据实际屏幕效果微调坐标**

烧录后在真实屏幕上对比设计稿，调整：
- 各元素 X/Y 对齐偏移
- 确保小时/分钟数字不超出屏幕
- 日期和星期在空白区域的位置
- 网格线和十字标记的位置

- [ ] **Step 2: 最终烧录验证**

```bash
idf.py -p /dev/tty.usbmodem* flash monitor
```

Expected: 表盘显示与设计稿基本一致，时间实时更新，布局完整无溢出。

- [ ] **Step 3: 最终提交**

```bash
git add main/
git commit -m "feat: finalize Pragmata watch face layout"
```
