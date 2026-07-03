# BSP_GPIO_EXTI 驱动文档

## 1. 概述

STM32F4/H7系列MCU通用的EXTI管理。

## 2. 架构设计

### 2.1 数据结构

#### EXTI_Callback_t - EXTI回调
```c
typedef struct {
    uint16_t pin;           /* GPIO引脚号 */
    void (*callback)(void); /* 回调函数 */
    uint8_t initialized;    /* 是否已初始化 */
} EXTI_Callback_t;
```

### 2.3 代码架构

```
内部辅助函数层：
├─ find_empty_slot()         // 寻找空槽位
└─ get_callback_index()      // 获取索引

公开API层：
├─ BSP_GPIO_EXTI_Register()     // 设备注册
└─ BSP_GPIO_EXTI_Unregister()   // 设备取消注册

HAL回调函数：
└─ HAL_GPIO_EXTI_Callback()
```

## 3. API接口说明

```c
uint8_t BSP_GPIO_EXTI_Register(uint16_t pin, void (*callback)(void));
```
```c
void BSP_GPIO_EXTI_Unregister(uint8_t index);
```

## 4. 注意事项
**重要**：这里回调是在ISR环境中，严禁任何延时行为，只允许信号量释放，消息队列等支持isr环境的操作

## 5. 版本历史

### v1.0 (2026-01-15)
- 初始版本
