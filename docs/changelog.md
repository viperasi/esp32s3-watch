# Pragmata Watch - 开发进度文档

> 更新日期: 2026-06-06

## 项目概述

基于 Waveshare ESP32-S3 Touch AMOLED 2.06" 开发板的智能手表项目，实现 NASA 蓝图（Pragmata）风格表盘界面。

## 硬件平台

| 项目 | 规格 |
|------|------|
| MCU | ESP32-S3, 240MHz, 16MB Flash, 8MB PSRAM (Octal) |
| 显示屏 | CO5300 QSPI AMOLED, 410x502 |
| 触摸 | FT3168 (I2C) |
| PMIC | AXP2101 (I2C) |
| RTC | PCF85063 (I2C) |
| IMU | QMI8658 (I2C) |
| Codec | ES8311 + ES7210 |
| 按钮 | BOOT (GPIO0, 低电平有效) / PWR (GPIO10, 高电平有效) |
| I2C 总线 | GPIO14(SCL) / GPIO15(SDA) 共享 |

## 软件栈

| 组件 | 版本/说明 |
|------|-----------|
| 开发框架 | ESP-IDF v5.4.2 |
| BSP | `waveshare/esp32_s3_touch_amoled_2_06` |
| UI 框架 | LVGL 9.3+ |
| 项目名称 | `pragmata_watch` |

## 已完成功能

### 1. 项目脚手架

- ESP-IDF 项目结构搭建
- 分区表: 8MB app + 7MB SPIFFS
- sdkconfig 配置: Octal PSRAM, QIO Flash, 240MHz, LVGL 优化
- BSP + LVGL 依赖 (`idf_component.yml`)
- 构建和烧录流程验证

### 2. Pragmata 表盘 UI

#### 视觉风格

NASA 工程蓝图美学:
- 工程网格背景 (40px 间距, 低透明度线条)
- 四角十字定位标记 (8px 臂长)
- 红 `#C0504D` / 蓝 `#4472C4` / 绿 `#4CAF50` 强调色
- 三色缝线分割条 (红-灰-蓝)

#### 设计区域

- 屏幕分辨率: 410x502
- 设计区域: 300x367 (居中偏移 55,67)
- 使用 `LV_OBJ_FLAG_OVERFLOW_VISIBLE` 允许大字体溢出容器

#### 布局结构 (从上到下)

```
┌──────────────────────────────────┐
│ PNL-042 · WATCH    REV.2026.06  │  顶部栏 (Montserrat 14)
│ ████████ ────────── ████████████ │  三色缝线分割条
│                                  │
│ HRS          │ DATE              │
│  21          │ 2026              │  小时行 (Barlow 172 + 38)
│              │ 06.05             │
│              │                   │
│ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │  分割线
│ DAY          │          MINUTES  │
│  FRI         │           32      │  分钟行 (Barlow 72 + 172)
│              │                   │
│ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ │  分割线
│ SEC  25      │ PWR SYS    80%   │  底部栏 (Space Mono 40)
│              │ ████████░░░░      │  电量条
│ ████████ ────────── ████████████ │  三色缝线分割条
│ ● SYS  ● NAV  ● COM  MEM 64K   │  状态栏 (指示灯 + 文字)
└──────────────────────────────────┘
```

#### 自定义字体

| 字体文件 | 字体族 | 大小 | 用途 |
|----------|--------|------|------|
| `font_barlow_172.c` | Barlow Condensed | 172px | 小时/分钟数字 |
| `font_barlow_72.c` | Barlow Condensed | 72px | 星期显示 |
| `font_barlow_38.c` | Barlow Condensed | 38px | 年份/月日 |
| `font_spacemono_40.c` | Space Mono | 40px | 秒数显示 |
| `font_spacemono_10.c` | Space Mono | 10px | (备用，当前未使用) |

装饰性标签使用 LVGL 内置 `lv_font_montserrat_14`。

### 3. 时间动态更新

- 统一 500ms LVGL 定时器驱动
- 正常模式每 1 秒刷新时间显示
- 启动时从编译时间戳 (`__DATE__` / `__TIME__`) 初始化系统时钟
- 时区设置为 UTC+8 (中国标准时间)

### 4. 双主题系统 (Light / Dark)

#### 配色方案

| 元素 | Light 模式 | Dark 模式 |
|------|-----------|-----------|
| 背景 | `#B0B0AC` | `#1A1F2E` |
| 主文字 | `#1A1F2E` | `#E0E0DC` |
| 次要文字 | `#555555` | `#AAAAAA` |
| 辅助文字 | `#888888` | `#777777` |
| 网格线 | 黑色, 20/255 透明 | 白色, 15/255 透明 |
| 分割线 | 黑色, 31/255 透明 | 白色, 25/255 透明 |
| 缝线灰色 | `#CCCCCC` | `#3A3F4E` |

#### 主题切换机制

- `theme_t` 结构体存储每套主题的完整配色
- `apply_theme()` 函数遍历所有 UI 对象更新颜色
- 背景网格和十字标记通过 `LV_EVENT_DRAW_MAIN` 回调绘制，自动读取当前主题色
- 切换时调用 `lv_obj_invalidate()` 触发重绘

### 5. 时间设置模式

#### 操作方式

| 操作 | 正常模式 | 设置模式 |
|------|---------|---------|
| 短按 BOOT | 切换 Light/Dark 主题 | 循环切换字段 (时→分→年→月→日) |
| 长按 BOOT (>800ms) | 进入设置模式 | 保存时间并退出 |
| 短按 PWR | 无响应 | 当前字段 +1 |

#### 设置模式特征

- 当前编辑字段以 500ms 频率闪烁 (文字色与背景色交替)
- 秒数区域显示 `SET:HRS` / `SET:MIN` 等提示当前字段
- 字段自动循环: 小时(0-23) → 分钟(0-59) → 年份(+1) → 月份(0-11) → 日期(1-31)
- 退出时通过 `settimeofday()` 写入系统时钟

#### 按键处理机制

- BOOT (GPIO0): GPIO 中断 ANYEDGE，ISR 记录按下时间戳，定时器中判断长按/短按
- PWR (GPIO10): GPIO 中断 POSEDGE，200ms 防抖
- 所有 LVGL 操作在定时器回调中执行 (非 ISR 上下文)

## 项目结构

```
test_watch/
├── CMakeLists.txt                    # 项目顶层 CMake
├── sdkconfig.defaults                # ESP-IDF 配置
├── partitions.csv                    # 分区表 (8M app + 7M spiffs)
├── main/
│   ├── CMakeLists.txt                # 注册源文件和字体
│   ├── idf_component.yml             # BSP + LVGL 依赖
│   ├── main.c                        # 入口: 时区/时间初始化, BSP启动
│   ├── watch_face.h                  # 表盘数据结构和接口
│   ├── watch_face.c                  # 表盘完整实现 (~685行)
│   └── fonts/                        # 自定义字体 (lv_font_conv 生成)
│       ├── font_barlow_172.c
│       ├── font_barlow_72.c
│       ├── font_barlow_38.c
│       ├── font_spacemono_40.c
│       └── font_spacemono_10.c
├── preview/                          # HTML 设计预览
│   ├── watch_face.html
│   └── watch_face_large_date.html
└── doc/
    └── board_pins.md                 # 引脚定义文档
```

## 编译和烧录

```bash
# 编译
cd your-project-dir
idf.py build

# 烧录 (替换为实际端口)
idf.py -p /dev/cu.usbmodem*** -b 115200 flash
```

## 技术要点

### 网格背景渲染

不使用 LVGL 对象创建网格线 (避免创建 45+ 对象导致白屏闪烁)，而是通过 `LV_EVENT_DRAW_MAIN` 事件回调直接在绘制层上渲染 1px 矩形:

```c
lv_obj_add_event_cb(scr, draw_bg_cb, LV_EVENT_DRAW_MAIN, NULL);
```

每次屏幕刷新时自动调用，根据当前主题色绘制网格和十字标记。

### Flex 布局

使用 LVGL 9 的 Flex 布局:
- 外层容器: `LV_FLEX_FLOW_COLUMN` 纵向排列
- 小时行/分钟行: `lv_obj_set_flex_grow(row, 115/105)` 按比例分配空间
- 内容通过 `lv_obj_align()` 绝对定位在 flex 行内部 (支持溢出)

### LVGL 定时器

单一 500ms 定时器处理所有逻辑:
- 按键事件检测 (长按/短按/PWR)
- 正常模式: 每 2 个 tick (1秒) 更新时间
- 设置模式: 每 tick (500ms) 切换闪烁状态
- 避免多定时器切换带来的上下文丢失问题

## 待开发功能

- [ ] SNTP 网络自动校时 (WiFi + NTP 服务器)
- [ ] AXP2101 电量读取和显示
- [ ] 触摸交互 (滑动切换表盘/功能)
- [ ] PCF85063 RTC 持久化时间
- [ ] 休眠/唤醒功耗管理
- [ ] 更多表盘样式
