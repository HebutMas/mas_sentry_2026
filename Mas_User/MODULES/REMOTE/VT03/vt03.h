#ifndef _VT03_H_
#define _VT03_H_

/* 按键位定义 */
#include "bsp_uart.h"
#include "module_offline.h"
#include "remote_data.h"
#include <stdint.h>

#define VT03_CH_VALUE_MIN    ((uint16_t)364)
#define VT03_CH_VALUE_OFFSET ((uint16_t)1024)
#define VT03_CH_VALUE_MAX    ((uint16_t)1684)

typedef struct
{
    int16_t          ch1;
    int16_t          ch2;
    int16_t          ch3;
    int16_t          ch4;
    int16_t          wheel;
    uint8_t          switch_pos;
    uint8_t          pause;
    uint8_t          custom_left;
    uint8_t          custom_right;
    uint8_t          trigger;
    mouse_state_t    mouse_state;
    keyboard_state_t key_state;
} vt03_remote_data_t;

typedef struct
{
    vt03_remote_data_t vt03_remote_data;
    OfflineDevice     *offline_dev; // 离线设备
    UART_Device       *uart_device; // UART实例
    uint8_t            initialized; // 初始化标志
} VT03_Instance_t;

/**
 * @description: vt03图传初始化
 * @return {UINT}，OSAL_SCUCESS 初始化成功
 */
UINT remote_vt03_init();
/**
 * @description: 获取vt03指针
 * @return {VT03_Instance_t *}，vt03指针
 */
VT03_Instance_t* remote_get_vt03();
/**
 * @description: vt03图传解码
 * @return {void}
 */
void remote_vt03_decode_task_func();
/**
 * @description: 获取vt03通道数据
 * @note 6-10为按键数据：switch_pos pause custom_left custom_right trigger
 * @param {uint8_t} channel_index，通道索引
 * @return {int16_t}，通道数据
 */
int16_t remote_get_vt03_channel(uint8_t channel_index);
/**
 * @description: 获取vt03的鼠标数据
 * @return {mouse_state_t*}，鼠标数据指针
 */
mouse_state_t *remote_vt03_get_mouse();
/**
 * @description: 获取vt03的键盘数据
 * @return {keyboard_state_t *}，键盘数据指针
 */
keyboard_state_t *remote_vt03_get_keyboard();

#endif // _VT03_H_