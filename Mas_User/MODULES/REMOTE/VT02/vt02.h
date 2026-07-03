/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-15 09:30:09
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-24 01:24:45
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/REMOTE/VT02/vt02.h
 * @Description:
 */
#ifndef _VT02_H_
#define _VT02_H_

#include "bsp_uart.h"
#include "module_offline.h"
#include "remote_data.h"
#include <stdint.h>

/* 帧头定义 */
typedef struct
{
    uint8_t  SOF;
    uint16_t DataLength;
    uint8_t  Seq;
    uint8_t  CRC8;
} VT02_Frame_Header;

typedef struct
{
    mouse_state_t    mouse_state;
    keyboard_state_t key_state;
} vt02_remote_data_t;

typedef struct
{
    VT02_Frame_Header  FrameHeader; // 接收到的帧头信息
    uint16_t           CmdID;
    vt02_remote_data_t vt02_remote_data;
    OfflineDevice     *offline_dev; // 离线设备
    UART_Device       *uart_device; // UART实例
    uint8_t            initialized; // 初始化标志
} VT02_Instance_t;

/**
 * @description: vt02图传初始化
 * @return {UINT}，OSAL_SCUCESS 初始化成功
 */
UINT remote_vt02_init();
/**
 * @description: 获取vt02指针
 * @return {VT02_Instance_t *}，vt02指针
 */
VT02_Instance_t* remote_get_vt02();
/**
 * @description: vt02图传解码
 * @return {void}
 */
void remote_vt02_decode_task_func();
/**
 * @description: 获取vt02的鼠标数据 
 * @return {mouse_state_t*}，鼠标数据指针
 */
mouse_state_t* remote_vt02_get_mouse();
/**
 * @description: 获取vt02的键盘数据
 * @return {keyboard_state_t *}，键盘数据指针
 */
keyboard_state_t* remote_vt02_get_keyboard();

#endif // _VT02_H_