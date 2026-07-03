# MODULE_RGB 模块文档

## 1. 概述

`module_rgb` 模块是一个兼容多平台的 LED 驱动模块。它根据硬件平台的不同，自动切换驱动方案：

* STM32H723xx: 使用 SPI 模拟单总线协议 驱动 WS2812 智能全彩 LED。
* STM32F407xx: 使用 三路 PWM 输出 驱动普通的模拟 RGB。

---
## 2. 架构设计


### 2.1 状态与宏定义
模块在 `module_rgb.h` 中预定义了常用颜色，方便直接调用：
| 颜色宏 | 十六进制值 | 视觉效果 |
| :--- | :--- | :--- |
| `LED_Red` | `0xFFFF0000` | 纯红 |
| `LED_Green` | `0xFF00FF00` | 纯绿 |
| `LED_Blue` | `0xFF0000FF` | 纯蓝 |
| `LED_Black` | `0xFF000000` | 熄灭 |
| `LED_White` | `0xFFFFFFFF` | 纯白 |

### 2.2 代码架构
```c
公开 API 层：
├─ Module_Rgb_init()        // RGB初始化
└─ Module_Rgb_show()        // RGB显示
```
## 3. 硬件实现原理

### 3.1 STM32H7 系列 (WS2812 SPI 模式)

WS2812 需要时序来区分物理 0 和 1。本模块利用 SPI 的高速数据传输，通过发送特定的字节（码型）来模拟单总线波形：

* 0 码 (`WS2812_LowLevel`): 发送 `0xC0`。
* 1 码 (`WS2812_HighLevel`): 发送 `0xF0`。

### 3.2 STM32F4 系列 (模拟 PWM 模式)

对于普通的 RGB 三色灯珠，模块初始化三路 PWM 通道，通过调整红、绿、蓝三色的占空比（0-1000）来实现全彩混合。
### 3.3 颜色模型 (ARGB)

模块采用 32 位 ARGB 颜色模型：`0xAARRGGBB`。

* Alpha (AA): 透明度/亮度通道。
* RGB 分别代表红、绿、蓝的分量。

---


## 4. API 接口说明

### 4.1 模块初始化

```c
void Module_Rgb_init(void);

```

### 4.2 显示控制

```c
void Module_Rgb_show(uint32_t aRGB);
```

---

## 5. 注意事项

* H7 SPI 频率: 使用 SPI 驱动 WS2812 时，需确保 SPI 波特率设置在 0.8Mhz - 1.4Mhz 之间，否则 WS2812 无法正确识别时序。
* F4 PWM 频率: 建议 PWM 频率在 1kHz 以上，以避免肉眼可见的闪烁。
* Alpha 通道: 如果只需要控制开关而不需要调光，将 Alpha 设为 `0xFF` 即可。

---

## 6. 使用示例
：

```c
// 1. 系统初始化阶段调用
Module_Rgb_init();

// 2. 显示红色 (满亮度)
Module_Rgb_show(LED_Red);

// 3. 显示半亮度的蓝色
// 透明度设置为 0x7F (约 50%)
Module_Rgb_show(0x7F0000FF);

// 4. 熄灭灯光
Module_Rgb_show(LED_Black);

```

---

## 7. 版本历史

### v1.0 (2026-02-10)

-初始版本 

---
