/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-23 16:19:49
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-24 11:13:44
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/REMOTE/remote_data.h
 * @Description:
 */
#ifndef _REMOTE_DATA_H_
#define _REMOTE_DATA_H_

#include <stdint.h>

#define REMOTE_SOURCE           1      // 遥控器选择 none 0 sbus 1 dt7 2
#define REMOTE_VT_SOURCE        0      // 图传选择   none 0 vt02 1 vt03 2
#define REMOTE_UART_RX_BUF_SIZE 32     // 遥控器接收缓冲区大小
#define REMOTE_DEAD_ZONE        10     // 遥控器死区范围

#if defined(STM32H723xx)
#define REMOTE_UART             huart5 // 遥控器串口
#define REMOTE_VT_UART          huart1 // 图传串口
#elif defined(STM32F407xx)
#define REMOTE_UART             huart3 // 遥控器串口
#define REMOTE_VT_UART          huart6 // 图传串口
#endif



/* 统一的按键类型定义 */
typedef enum
{
    KEY_W = 0,
    KEY_S,
    KEY_A,
    KEY_D,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_Q,
    KEY_E,
    KEY_R,
    KEY_F,
    KEY_G,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_COUNT // 用于计数
} remote_key_e;

/* keyboard state structure */
typedef union
{
    uint16_t key_code;
    struct
    {
        uint16_t KEY_W     : 1;
        uint16_t KEY_S     : 1;
        uint16_t KEY_A     : 1;
        uint16_t KEY_D     : 1;
        uint16_t KEY_SHIFT : 1;
        uint16_t KEY_CTRL  : 1;
        uint16_t KEY_Q     : 1;
        uint16_t KEY_E     : 1;
        uint16_t KEY_R     : 1;
        uint16_t KEY_F     : 1;
        uint16_t KEY_G     : 1;
        uint16_t KEY_Z     : 1;
        uint16_t KEY_X     : 1;
        uint16_t KEY_C     : 1;
        uint16_t KEY_V     : 1;
        uint16_t KEY_B     : 1;
    } bit;
} keyboard_state_t;

typedef struct
{
    int16_t mouse_x;
    int16_t mouse_y;
    int16_t mouse_z;
    uint8_t mouse_l;
    uint8_t mouse_r;
    uint8_t mouse_m;
} mouse_state_t;

#endif // _REMOTE_DATA_H_