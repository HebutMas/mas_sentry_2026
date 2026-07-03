#ifndef _DT7_H_
#define _DT7_H_

#include "bsp_uart.h"
#include "module_offline.h"
#include "remote_data.h"
#include <stdint.h>

#define DT7_CH_VALUE_MIN    ((uint16_t)364)
#define DT7_CH_VALUE_OFFSET ((uint16_t)1024)
#define DT7_CH_VALUE_MAX    ((uint16_t)1684)

#define DT7_SW_UP           ((uint16_t)1) // 开关向上时的值
#define DT7_SW_MID          ((uint16_t)3) // 开关中间时的值
#define DT7_SW_DOWN         ((uint16_t)2) // 开关向下时的值

typedef struct
{
    int16_t          ch1;
    int16_t          ch2;
    int16_t          ch3;
    int16_t          ch4;
    uint8_t          sw1;
    uint8_t          sw2;
    mouse_state_t    mouse_state;
    keyboard_state_t keyboard_state;
    int16_t          wheel;
} DT7_INPUT_t;

typedef struct
{
    DT7_INPUT_t    dt7_input;
    OfflineDevice *offline_dev; // 离线设备
    UART_Device   *uart_device; // UART实例
    uint8_t        initialized; // 初始化标志
} DT7_Instance_t;

/**
 * @description: dt7遥控器初始化
 * @return {UINT}，TX_SSCUCCESS初始化成功
 */
UINT remote_dt7_init();
/**
 * @description: 获取dt7指针
 * @return {DT7_Instance_t *}，dt7指针
 */
DT7_Instance_t* remote_get_dt7();
/**
 * @description: dt7数据解码
 * @param {uint8_t} *buf，数据指针
 */
void remote_dt7_decode_task_func();
/**
 * @description: 获取dt7通道数据
 * @param {uint8_t} channel_index，通道索引
 * @return {int16_t}，通道数据
 */
int16_t remote_get_dt7_channel(uint8_t channel_index);
/**
 * @description: 获取dt7的鼠标数据 
 * @return {mouse_state_t*}，鼠标数据指针
 */
mouse_state_t* remote_dt7_get_mouse();
/**
 * @description: 获取dt7的键盘数据
 * @return {keyboard_state_t *}，键盘数据指针
 */
keyboard_state_t* remote_dt7_get_keyboard();
#endif // _DT7_H_