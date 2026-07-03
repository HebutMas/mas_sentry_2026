/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-27 19:55:26
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-10 00:47:03
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\WT606\module_wt606.h
 * @Description:
 */
#ifndef _MODULE_WT606_H_
#define _MODULE_WT606_H_

#include "bsp_uart.h"
#include "module_offline.h"
#include <stdbool.h>
#include <stdint.h>

// 这里根据需要自定义修改
#if defined(STM32H723xx)
#define WT606_UART huart10
#elif defined(STM32F407xx)
#define WT606_UART huart1
#endif
// 离线beep蜂鸣次数
#define WT606_OFFLINE_BEEP_TIMES 1

typedef struct
{
    uint8_t        initialized;
    float          acc[3];
    float          gyro_dps[3];
    float          gyro_rps[3];
    float          Euler_degree[3]; // roll pitch yaw
    float          Euler_rad[3];    // roll pitch yaw
    float          YawTotalAngle_degree;
    float          YawTotalAngle_rad;
    int32_t        YawRoundCount;
    float          last_yaw_degree;
    UART_Device   *uart_dev;
    OfflineDevice *offline_dev;
} Module_WT606_Device_t;

UINT Module_WT606_init();

void Module_WT606_task_function();

Module_WT606_Device_t *Module_WT606_get_device();

#endif // _MODULE_WT606_H_