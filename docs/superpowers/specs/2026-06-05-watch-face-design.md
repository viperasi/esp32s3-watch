# ESP32-S3 Pragmata Watch Face Design

## Overview

基于 Waveshare ESP32-S3 Touch AMOLED 2.06" 开发板的智能手表项目，第一版实现 NASA 蓝图（Pragmata）风格表盘界面。

## Hardware

- MCU: ESP32-S3, 240MHz, 16MB Flash, 8MB PSRAM (Octal)
- Display: CO5300 QSPI AMOLED, 410x502
- Touch: FT3168 (I2C)
- PMIC: AXP2101 (I2C)
- RTC: PCF85063 (I2C)
- IMU: QMI8658 (I2C)
- Codec: ES8311 + ES7210
- SD Card: SPI
- 所有 I2C 设备共享 GPIO14(SCL)/GPIO15(SDA)

## Software Stack

- ESP-IDF v5.4.2
- BSP: `waveshare/esp32_s3_touch_amoled_2_06`
- UI Framework: LVGL 9.3+
- Fonts: Space Mono (等宽) + Barlow Condensed (显示)

## Watch Face Design

### Visual Style: Pragmata (NASA Blueprint)

参考 Hugo Pragmata 主题的工程蓝图美学：

- **配色**: 浅灰绿工程纸 `#E8E8E4` 背景，深蓝黑 `#1a1f2e` 文字
- **网格**: 20px 工程图纸网格背景
- **十字标记**: 四角定位标记 (SVG line)
- **分割线**: 红 `#C0504D` / 灰 `#CCCCCC` / 蓝 `#4472C4` 三色缝线分割条
- **状态灯**: 绿(SYS) / 蓝(NAV) / 红(COM) 圆点指示灯

### Layout (410x502)

```
┌──────────────────────────────────┐
│ PNL-042 · WATCH    REV.2026.06  │  顶部栏
│ ████████ ────────── ████████████ │  红白蓝分割线
│                                  │
│ HRS          │ DATE              │
│  21          │ 2026              │  上半区
│              │ 06.05             │  小时172px + 日期38px
│              │                   │
│ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │  分割线
│ DAY          │          MINUTES  │
│  FRI         │           32      │  下半区
│              │                   │  星期72px(蓝) + 分钟172px
│ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │  分割线
│ SEC  25      │ PWR SYS    80%   │  底部栏
│              │ ████████░░░░      │  秒(红40px) + 电量条
│ ████████ ────────── ████████████ │  红白蓝分割线
│ ● SYS  ● NAV  ● COM  MEM 64K   │  状态栏
└──────────────────────────────────┘
```

### Data Elements

| Element | Size | Color | Position | Source |
|---------|------|-------|----------|--------|
| Hours | 172px | #1a1f2e | 左上 | system time |
| Minutes | 172px | #1a1f2e | 右下 | system time |
| Date (Year) | 38px | #1a1f2e | 右上 | system time |
| Date (MD) | 38px | #555 | 右上 | system time |
| Day | 72px | #4472C4 | 左下 | system time |
| Seconds | 40px | #C0504D | 左下底栏 | system time |
| Battery % | 8px | #1a1f2e | 右下底栏 | AXP2101 |
| Battery bar | 7px | #4472C4 | 右下底栏 | AXP2101 |

### Dynamic Behavior

- 时间（时/分/秒）每秒更新
- 日期/星期每分钟检查更新
- 电量每 30 秒从 AXP2101 读取更新
- 秒数使用红色强调色，与蓝色星期形成红蓝对比

## Project Structure

```
test_watch/
├── CMakeLists.txt
├── sdkconfig.defaults          # Flash/PSRAM/屏幕/LVGL 配置
├── partitions.csv              # 8M app + 7M spiffs
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml       # BSP + LVGL 9
│   ├── main.c                  # 入口：BSP初始化 + LVGL启动 + 创建表盘
│   ├── watch_face.h            # 表盘创建/更新接口
│   └── watch_face.c            # 表盘UI实现
```

## Dependencies (idf_component.yml)

```yaml
dependencies:
  waveshare/esp32_s3_touch_amoled_2_06: "*"
  lvgl/lvgl: ">=9.3.0"
```

## Implementation Notes

- 使用 `bsp_display_start()` 初始化屏幕和触摸
- 使用 `bsp_display_lock/unlock()` 保护 LVGL 操作
- LVGL 定时器 (`lv_timer`) 每秒更新时间显示
- AXP2101 电量通过 BSP 的电源管理接口读取
- 字体：使用 LVGL 内置字体或自定义编译 Space Mono / Barlow Condensed
- 分割线用 `lv_obj` 设置背景色实现
- 网格背景用 Canvas 或预渲染图片绘制一次
- 十字标记用 `lv_line` 绘制
