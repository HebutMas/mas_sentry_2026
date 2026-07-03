/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-15 21:20:20
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-29 22:25:42
 * @FilePath: /MAS_RM_DJIC/Mas_User/BSP/CAN/bsp_can.h
 * @Description:
 */
#ifndef _BSP_CAN_H_
#define _BSP_CAN_H_

#if defined(STM32H723xx)
#include "fdcan.h"
#elif defined(STM32F407xx)
#include "can.h"
#endif

#include "tx_api.h"
#include "tx_port.h"
#include <stdint.h>

/* CAN设备实例结构体 */
typedef struct Can_Device
{
#if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan; // CAN句柄
#elif defined(STM32F407xx)
    CAN_HandleTypeDef *hcan; // CAN句柄
#endif
    uint32_t           valid;             // 设备指针有效性检测标志
    uint32_t           tx_id;             // 发送id
    uint32_t           rx_id;             // 接收id
    uint8_t            rx_buf[8];         // 接收缓冲区
    uint8_t            real_rx_len;       // 实际接收长度
    uint32_t           eventflag;         // 事件
    uint8_t            filter_bank_index; // 占用的硬件过滤器编号
    struct Can_Device *next;              // 链表下一个节点指针
} Can_Device;

/* 初始化配置结构体 */
typedef struct
{
#if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan; // CAN句柄
#elif defined(STM32F407xx)
    CAN_HandleTypeDef *hcan; // CAN句柄
#endif
    uint32_t tx_id;
    uint32_t rx_id;
} Can_Device_Init_Config_s;

/* CAN总线管理结构 */
typedef struct
{
#if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan; // CAN句柄
#elif defined(STM32F407xx)
    CAN_HandleTypeDef *hcan; // CAN句柄
#endif
    Can_Device *device_list; // 设备链表头指针
} CAN_Bus_Manager;

typedef struct
{
#if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan; // CAN句柄
#elif defined(STM32F407xx)
    CAN_HandleTypeDef *hcan; // CAN句柄
#endif
    uint32_t             id;
    uint8_t              len;
    uint8_t              buf[8];
} CanTxMessage_t;

/**
 * @description: 初始化can全局事件
 * @return {*}
 */
void Can_Event_init(void);
/**
 * @description: 初始化CAN设备
 * @param {Can_Device_Init_Config_s*} config
 * @return {Can_Device*}，CAN设备指针
 */
Can_Device *BSP_CAN_Device_Init(Can_Device_Init_Config_s *config);
/**
 * @description:  销毁CAN设备实例
 * @param {Can_Device*} dev，要销毁的设备实例
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_CAN_Device_DeInit(Can_Device *dev);
/**
 * @description: 发送CAN消息
 * @note  注意这个函数默认发送标准can（IDE = CAN_ID_STD，RTR= CAN_RTR_DATA）
 * @param {Can_Device*} device，can设备指针
 * @param {uint8_t*} data，发送数据指针
 * @param {uint8_t} len,发送长度
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_CAN_SendDevice(Can_Device *device, uint8_t *data, uint8_t len);
/**
 * @description: 发送CAN消息
 * @note  注意这个函数默认发送标准can（IDE = CAN_ID_STD，RTR= CAN_RTR_DATA）
 * @param {CanTxMessage_t *}，CAN发送消息结构体指针
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_CAN_SendMessage(CanTxMessage_t* cantxmeaasge);
/**
 * @description: 读取单个CAN设备数据
 * @param {Can_Device*} device
 * @param {uint8_t*} data，接收数据指针
 * @param {uint32_t} timeout - 超时时间
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_CAN_ReadSingleDevice(Can_Device *device, uint32_t timeout);
/**
 * @description: 读取多个CAN设备数据
 * @param {uint32_t} eventflags - 组合好的事件标志
 * @param {uint8_t*} data，接收数据指针
 * @param {uint32_t} timeout - 超时时间
 * @return {uint32_t} 返回触发的eventlfag
 */
uint32_t BSP_CAN_ReadMultipleDevice(uint32_t eventflags, uint32_t timeout);

#endif // _BSP_CAN_H_