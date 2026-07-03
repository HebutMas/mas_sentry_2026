/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-15 19:46:02
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-15 20:53:22
 * @FilePath: /MAS_RM_DJIC/Mas_User/BSP/GPIO/bsp_gpio.c
 * @Description: 
 */

#include "bsp_gpio.h"
#include <string.h>

#define LOG_TAG "bsp_gpio"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

/* 最大EXTI回调数量 */
#define MAX_EXTI_CALLBACKS 16

/* 存储EXTI回调信息的结构体 */
typedef struct {
    uint16_t pin;           /* GPIO引脚号 */
    void (*callback)(void); /* 回调函数 */
    uint8_t initialized;    /* 是否已初始化 */
} EXTI_Callback_t;

/* 存储EXTI回调的静态数组 */
static EXTI_Callback_t exti_callbacks[MAX_EXTI_CALLBACKS];
static uint8_t callback_count = 0;

// 内部函数
static uint8_t find_empty_slot(void) {
    for (uint8_t i = 0; i < MAX_EXTI_CALLBACKS; i++) {
        if (!exti_callbacks[i].initialized) {
            return i;
        }
    }
    return MAX_EXTI_CALLBACKS;
}

static uint8_t get_callback_index(uint16_t pin) {
    for (uint8_t i = 0; i < callback_count; i++) {
        if (exti_callbacks[i].pin == pin) {
            return i;
        }
    }
    return MAX_EXTI_CALLBACKS;
}

// 对外接口
uint8_t BSP_GPIO_EXTI_Register(uint16_t pin, void (*callback)()) {
    uint8_t index;
    
    /* 验证参数 */
    if (callback == NULL) {
        log_e("EXTI registration failed: NULL callback function");
        return 0xFF;
    }
    
    if (pin > 15) {
        log_e("EXTI registration failed: Invalid pin number %u", pin);
        return 0xFF;
    }
    
    /* 检查是否已注册 */
    if (get_callback_index(pin) < MAX_EXTI_CALLBACKS) {
        log_w("EXTI already registered for pin %u", pin);
        return get_callback_index(pin);
    }
    
    /* 查找空槽 */
    index = find_empty_slot();
    if (index >= MAX_EXTI_CALLBACKS) {
        log_e("EXTI registration failed: Maximum callback count reached");
        return 0xFF;
    }
    
    /* 存储回调信息 */
    exti_callbacks[index].pin = pin;
    exti_callbacks[index].callback = callback;
    exti_callbacks[index].initialized = 1;
    
    callback_count++;
    
    log_i("EXTI registered for pin %u, callback count: %u", pin, callback_count);
    
    return index;
}

void BSP_GPIO_EXTI_Unregister(uint8_t index) {
    if (index >= MAX_EXTI_CALLBACKS) {
        log_e("EXTI unregistration failed: Invalid index %u", index);
        return;
    }
    
    if (!exti_callbacks[index].initialized) {
        log_w("EXTI unregistration failed: Callback at index %u not registered", index);
        return;
    }
    
    /* 清除回调信息 */
    exti_callbacks[index].pin = 0;
    exti_callbacks[index].callback = NULL;
    exti_callbacks[index].initialized = 0;
    
    callback_count--;
    
    log_i("EXTI unregistered, callback count: %u", callback_count);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    uint8_t index = get_callback_index(GPIO_Pin);
    
    if (index < MAX_EXTI_CALLBACKS && exti_callbacks[index].callback) {
        exti_callbacks[index].callback();
    }
}
