#ifndef _SBUS_H_
#define _SBUS_H_

#include "bsp_uart.h"
#include "module_offline.h"
#include <stdint.h>

#define SBUS_CHX_BIAS ((uint16_t)1024)
#define SBUS_CHX_UP   ((uint16_t)240)
#define SBUS_CHX_DOWN ((uint16_t)1807)

typedef struct
{
    int16_t  CH1;          // 1通道
    int16_t  CH2;          // 2通道
    int16_t  CH3;          // 3通道
    int16_t  CH4;          // 4通道
    uint16_t CH5;          // 5通道
    uint16_t CH6;          // 6通道
    uint16_t CH7;          // 7通道
    uint16_t CH8;          // 8通道
    uint16_t CH9;          // 9通道
    uint16_t CH10;         // 10通道
    uint16_t CH11;         // 11通道
    uint16_t CH12;         // 12通道
    uint16_t CH13;         // 13通道
    uint16_t CH14;         // 14通道
    uint16_t CH15;         // 15通道
    uint16_t CH16;         // 16通道
    uint8_t  ConnectState; // 连接标志
} SBUS_CH_Struct;

typedef struct
{
    SBUS_CH_Struct SBUS_CH;
    OfflineDevice *offline_dev; // 离线设备
    UART_Device   *uart_device; // UART实例
    uint8_t        initialized; // 初始化标志
} SBUS_Instance_t;

/**
 * @description: sbus初始化
 * @return {UINT}，TX_SCUCCESS初始化成功,其余失败
 */
UINT remote_sbus_init();
/**
 * @description: 获取sbus指针
 * @return {SBUS_Instance_t *}，sbus指针
 */
SBUS_Instance_t* remote_get_sbus();
/**
 * @description: sbus数据解码
 * @param {uint8_t} *buf，缓冲区指针
 * @return {*}
 */
void remote_sbus_decode_task_func();
/**
 * @description: 获取sbus通道状态
 * @param {uint8_t} channel_index，通道索引
 * @return {int16_t}，通道数据
 */
int16_t remote_get_sbus_channel(uint8_t channel_index);

#endif // _SBUS_H_