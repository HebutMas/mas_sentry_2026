/*
 * @Author: laladuduqq 280752397@qq.com
 * @Date: 2025-09-09 17:03:48
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-29 22:37:17
 * @FilePath: /MAS_RM_DJIC/Mas_User/BSP/CAN/bsp_can_f4.c
 * @Description:
 */

#include "bsp_can.h"
#include "bsp_def.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "bsp_can"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define CAN_BUS_NUM            2
#define CAN_DEVICE_VALID_CKECK 0xABCDEF01

// CAN总线管理器数组
static CAN_Bus_Manager can_bues[CAN_BUS_NUM];
// 全局CAN事件组
static TX_EVENT_FLAGS_GROUP can_event;
// CAN过滤器索引
static uint8_t can1_filter_idx = 0, can2_filter_idx = 14; // 0-13给can1用,14-27给can2用

// 用于判断是否在定时器回调中
extern TX_THREAD _tx_timer_thread;

/**
 * @description: 添加CAN过滤器
 * @param {CAN_HandleTypeDef*} hcan CAN句柄
 * @param {uint32_t} rx_id 接收ID
 * @param {uint8_t*} out_bank_index 输出分配到的过滤器编号
 * @return {bool} 成功返回true，失败返回false
 */
static bool CANConfigFilter(CAN_HandleTypeDef *hcan, uint32_t rx_id, uint8_t *out_bank_index)
{
    CAN_FilterTypeDef can_filter_conf;
    uint8_t           filter_bank;

    if (hcan == &hcan1)
    {
        if (can1_filter_idx >= 14)
            return false;
        filter_bank = can1_filter_idx++;
    }
    else if (hcan == &hcan2)
    {
        if (can2_filter_idx >= 28)
            return false;
        filter_bank = can2_filter_idx++;
    }
    else
    {
        return false;
    }

    // 模式设为掩码模式精准匹配
    can_filter_conf.FilterMode = CAN_FILTERMODE_IDMASK;
    // 16位缩放模式
    can_filter_conf.FilterScale = CAN_FILTERSCALE_16BIT;
    // FIFO 分配逻辑
    can_filter_conf.FilterFIFOAssignment = (rx_id & 1) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
    // 从机模式起始过滤器组
    can_filter_conf.SlaveStartFilterBank = 14;
    // 配置 Filter Bank (低位过滤器) - 精准匹配 rx_id
    can_filter_conf.FilterIdLow     = rx_id << 5;
    can_filter_conf.FilterMaskIdLow = 0x7FF << 5;
    // 配置 Filter Bank (高位过滤器) - 精准匹配 rx_id
    can_filter_conf.FilterIdHigh     = rx_id << 5;
    can_filter_conf.FilterMaskIdHigh = 0x7FF << 5;

    can_filter_conf.FilterBank       = filter_bank;
    can_filter_conf.FilterActivation = CAN_FILTER_ENABLE;

    if (HAL_CAN_ConfigFilter(hcan, &can_filter_conf) != HAL_OK)
    {
        // 失败时回退索引
        if (hcan == &hcan1)
            can1_filter_idx--;
        else
            can2_filter_idx--;
        return false;
    }

    if (out_bank_index)
        *out_bank_index = filter_bank;

    return true;
}
/**
 * @description: 检查设备ID冲突
 * @param {CANBusManager*} bus
 * @param {uint16_t} tx_id
 * @param {uint16_t} rx_id
 * @return {bool} true表示有冲突，false表示无冲突
 */
static bool check_device_id_conflict(CAN_Bus_Manager *bus, uint16_t tx_id, uint16_t rx_id)
{
    Can_Device *current = bus->device_list;

    while (current != NULL)
    {
        if (current->rx_id == rx_id)
        {
            return true;
        }
        current = current->next;
    }
    return false;
}

static CAN_Bus_Manager *BSP_CAN_Get_Bus_Manager(CAN_HandleTypeDef *hcan)
{
    if (hcan == NULL)
    {
        log_e("NULL hcan parameter");
        return NULL;
    }

    for (int i = 0; i < CAN_BUS_NUM; i++)
    {
        if (can_bues[i].hcan == hcan || can_bues[i].hcan == NULL)
        {
            return &can_bues[i];
        }
    }

    return NULL;
}

uint32_t BSP_CAN_Get_Event_Flag(void)
{
    static uint8_t counter = 0;

    if (counter >= 32)
    {
        return 0; // 所有标志位都已分配
    }

    uint32_t flag = (1U << counter);
    counter++;

    return flag;
}
// 对外接口
void Can_Event_init(void)
{
    // 创建总线事件
    if (tx_event_flags_create(&can_event, "can_event") != TX_SUCCESS)
    {
        log_e("Failed to create can event");
    }
}

Can_Device *BSP_CAN_Device_Init(Can_Device_Init_Config_s *config)
{
    if (config == NULL || config->hcan == NULL)
    {
        return NULL;
    }

    // 获取或初始化总线管理器
    CAN_Bus_Manager *bus_manager = BSP_CAN_Get_Bus_Manager(config->hcan);
    if (bus_manager == NULL)
    {
        log_e("bus_manager address is NULL");
        return NULL;
    }

    // 初始化总线管理器（如果尚未初始化）
    if (bus_manager->hcan == NULL)
    {
        bus_manager->hcan        = config->hcan;
        bus_manager->device_list = NULL;
    }

    // 检查ID冲突
    if (check_device_id_conflict(bus_manager, config->tx_id, config->rx_id))
    {
        return NULL;
    }

    // 分配新设备内存
    Can_Device *dev = NULL;
    BSP_MEM_ALLOC_WAIT(dev, sizeof(Can_Device), TX_NO_WAIT);
    if (dev == NULL)
    {
        log_e("Failed to allocate memory for CAN device");
        return NULL;
    }

    // 配置设备参数
    dev->valid = CAN_DEVICE_VALID_CKECK;
    dev->hcan  = config->hcan;
    dev->tx_id = config->tx_id;
    dev->rx_id = config->rx_id;
    dev->next  = NULL;

    // 分配事件标志
    dev->eventflag = BSP_CAN_Get_Event_Flag();
    if (dev->eventflag == 0)
    {
        log_e("flag used out");
        // 标记设备为无效
        dev->valid = 0x00000000;
        BSP_MEM_FREE(dev);
        return NULL;
    }

    // 添加CAN过滤器
    if (CANConfigFilter(dev->hcan, dev->rx_id, &dev->filter_bank_index) != true)
    {
        log_e("add can filter failed");
        // 标记设备为无效
        dev->valid = 0x00000000;
        BSP_MEM_FREE(dev);
        return NULL;
    }

    // 将设备添加到链表头部
    dev->next                = bus_manager->device_list;
    bus_manager->device_list = dev;

    // 启动CAN接收中断
    HAL_CAN_ActivateNotification(config->hcan,
                                 CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING);
    // 启动总线
    HAL_CAN_Start(config->hcan);

    log_i("CAN device initialized successfully");

    return dev;
}

UINT BSP_CAN_Device_DeInit(Can_Device *dev)
{
    if (dev == NULL)
    {
        log_e("Device pointer is NULL");
        return TX_DELETED;
    }

    // 验证设备有效性
    if (dev->valid != CAN_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    CAN_Bus_Manager *bus_manager = BSP_CAN_Get_Bus_Manager(dev->hcan);
    if (bus_manager == NULL)
    {
        log_e("Device bus_manager is NULL");
        return TX_DELETED;
    }

    // 禁用并清理硬件过滤器
    CAN_FilterTypeDef can_filter_conf;
    can_filter_conf.FilterBank       = dev->filter_bank_index;
    can_filter_conf.FilterMode       = CAN_FILTERMODE_IDMASK;
    can_filter_conf.FilterScale      = CAN_FILTERSCALE_16BIT;
    can_filter_conf.FilterActivation = CAN_FILTER_DISABLE; // 禁用过滤器
    HAL_CAN_ConfigFilter(dev->hcan, &can_filter_conf);

    // 从链表中移除设备
    if (bus_manager->device_list == dev)
    {
        // 设备是头节点
        bus_manager->device_list = dev->next;
    }
    else
    {
        // 查找设备的前一个节点
        Can_Device *current = bus_manager->device_list;
        while (current != NULL && current->next != dev)
        {
            current = current->next;
        }

        if (current != NULL)
        {
            // 找到设备的前一个节点，移除当前设备
            current->next = dev->next;
        }
    }

    // 标记设备为无效
    dev->valid = 0x00000000;

    // 释放设备内存
    BSP_MEM_FREE(dev);

    // 如果没有设备了，清理总线管理器资源
    if (bus_manager->device_list == NULL)
    {
        bus_manager->hcan = NULL;
    }

    log_i("CAN device deinitialized successfully");
    return TX_SUCCESS;
}

UINT BSP_CAN_SendDevice(Can_Device *device, uint8_t *data, uint8_t len)
{
    if (device == NULL || data == NULL || len == 0 || len > 8)
    {
        return TX_DELETED;
    }

    CAN_Bus_Manager *bus_manager = BSP_CAN_Get_Bus_Manager(device->hcan);
    if (bus_manager == NULL)
    {
        log_e("Device bus_manager is NULL");
        return TX_DELETED;
    }

    CAN_TxHeaderTypeDef txheader;
    txheader.StdId = device->tx_id;
    txheader.DLC = len;
    txheader.RTR = CAN_RTR_DATA;
    txheader.IDE = CAN_ID_STD;

    uint32_t mailbox;
    HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(device->hcan, &txheader, data,&mailbox);

    if (status != HAL_OK)
    {
        return TX_DELETED;
    }

    return TX_SUCCESS;
}

UINT BSP_CAN_SendMessage(CanTxMessage_t* cantxmeaasge)
{
    if (cantxmeaasge == NULL)
    {
        return TX_DELETED;
    }

    CAN_Bus_Manager *bus_manager = BSP_CAN_Get_Bus_Manager(cantxmeaasge->hcan);
    if (bus_manager == NULL)
    {
        log_e("Device bus_manager is NULL");
        return TX_DELETED;
    }

    CAN_TxHeaderTypeDef txheader;
    txheader.StdId = cantxmeaasge->id;
    txheader.DLC = cantxmeaasge->len;
    txheader.RTR = CAN_RTR_DATA;
    txheader.IDE = CAN_ID_STD;


    uint32_t mailbox;
    HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(cantxmeaasge->hcan, &txheader, cantxmeaasge->buf,&mailbox);


    if (status != HAL_OK)
    {
        return TX_DELETED;
    }

    return TX_SUCCESS;
}

UINT BSP_CAN_ReadSingleDevice(Can_Device *device, uint32_t timeout)
{
    if (device == NULL)
    {
        return TX_DELETED;
    }

    ULONG actual_flags;
    UINT  status =
        tx_event_flags_get(&can_event, device->eventflag, TX_OR_CLEAR, &actual_flags, timeout);

    if (status == TX_SUCCESS)
    {
        return TX_SUCCESS;
    }

    return TX_DELETED;
}

uint32_t BSP_CAN_ReadMultipleDevice(uint32_t eventflags, uint32_t timeout)
{
    if (eventflags == 0)
    {
        return 0;
    }

    ULONG actual_flags;
    UINT  status = tx_event_flags_get(&can_event, eventflags, TX_OR_CLEAR, &actual_flags, timeout);

    if (status == TX_SUCCESS)
    {
        return actual_flags & eventflags;
    }
    return 0;
}

/**
 * @description: CAN接收中断回调函数
 * @param {CAN_HandleTypeDef*} hcan
 * @param {uint32_t} RxFifo
 * @return {*}
 */
static void BSP_CAN_RxCallback(CAN_HandleTypeDef *hcan, uint32_t RxFifo)
{
    // 查找对应的总线管理器
    CAN_Bus_Manager *bus_manager = BSP_CAN_Get_Bus_Manager(hcan);
    if (bus_manager == NULL)
    {
        return;
    }

    // 处理接收到的消息
    CAN_RxHeaderTypeDef rx_header;
    uint8_t             rx_data[8];

    while (HAL_CAN_GetRxFifoFillLevel(hcan, RxFifo) > 0)
    {
        if (HAL_CAN_GetRxMessage(hcan, RxFifo, &rx_header, rx_data) == HAL_OK)
        {
            // 查找对应的设备
            Can_Device *current = bus_manager->device_list;
            while (current != NULL)
            {
                if (current->rx_id == rx_header.StdId)
                {
                    // 更新设备缓冲区
                    memcpy(current->rx_buf, rx_data, rx_header.DLC);
                    current->real_rx_len = rx_header.DLC;
                    // 设置设备事件标志
                    tx_event_flags_set(&can_event, current->eventflag, TX_OR);
                    break;
                }
                current = current->next;
            }
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    BSP_CAN_RxCallback(hcan, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    BSP_CAN_RxCallback(hcan, CAN_RX_FIFO1);
}
