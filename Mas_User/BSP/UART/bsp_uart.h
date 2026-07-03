/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 14:26:54
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-15 10:59:34
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/BSP/UART/bsp_uart.h
 * @Description:
 */
#ifndef _BSP_UART_H_
#define _BSP_UART_H_

#include "tx_api.h"
#include "usart.h"

/* UART事件定义 */
#define UART_RX_DONE_EVENT (0x01 << 0) // 接收完成事件
#define UART_TX_DONE_EVENT (0x01 << 1) // 发送完成事件
#define UART_ERROR_EVENT   (0x01 << 2) // 错误事件

// 模式选择
typedef enum
{
    UART_MODE_BLOCKING,
    UART_MODE_IT,
    UART_MODE_DMA
} UART_Mode;

// UART设备结构体
typedef struct UART_Device
{
    uint32_t            valid;            // 设备指针有效性检测（0xABCDEF01）
    UART_HandleTypeDef *huart;            // UART句柄
    uint8_t (*rx_buf)[2];                 // 指向外部定义的双缓冲区，32字节对齐
    uint16_t             rx_buf_size;     // 缓冲区大小
    volatile uint8_t     rx_active_buf;   // 当前活动缓冲区
    uint16_t             real_rx_len;     // 实际接收数据长度
    uint16_t             expected_rx_len; // 预期长度（0为不定长）
    TX_EVENT_FLAGS_GROUP uart_event;      // uart事件
    UART_Mode            rx_mode;         // 接收模式
    UART_Mode            tx_mode;         // 发送模式
    struct UART_Device  *next;            // 下一个节点指针
} UART_Device;

// 初始化配置结构体
typedef struct
{
    UART_HandleTypeDef *huart; // UART句柄
    uint8_t (*rx_buf)[2];      // 外部接收缓冲区指针，32字节对齐
    uint16_t  rx_buf_size;     // 接收缓冲区大小
    uint16_t  expected_rx_len; // 预期长度（0为不定长）
    UART_Mode rx_mode;         // 接收模式
    UART_Mode tx_mode;         // 发送模式
} UART_Device_init_config;

/**
 * @description: UART初始化函数
 * @details      初始化UART设备
 * @param        device：UART_Device指针
 * @param        config：UART_Device_init_config指针
 * @return       UART_Device*：初始化成功返回UART_Device指针，失败返回NULL
 */
UART_Device *BSP_UART_Device_Init(UART_Device_init_config *config);
/**
 * @description: UART发送函数
 * @details      发送数据到UART设备，dma模式下要求数据32字节对齐
 * @param        device：UART_Device指针
 * @param        data：要发送的数据指针
 * @param        len：数据长度
 * @param        timeout：超时时间
 * @return       int：实际发送的字节数
 */
int BSP_UART_Send(UART_Device *device, uint8_t *data, uint16_t len, uint32_t timeout);
/**
 * @description: UART接收函数
 * @details      从UART设备接收数据
 * @param        device：UART_Device指针
 * @param        timeout：超时时间
 * @return       uint8_t*：指向接收数据的指针，失败返回NULL
 */
uint8_t *BSP_UART_Read(UART_Device *device, uint32_t timeout);
/**
 * @description: UART反初始化函数
 * @details      释放UART设备资源，删除事件标志组和信号量
 * @param        device：UART_Device指针
 * @return       UINT：TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_UART_Deinit(UART_Device *device);

#endif // _BSP_UART_H_
