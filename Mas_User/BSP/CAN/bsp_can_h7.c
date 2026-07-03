/*
 * @Author: laladuduqq 280752397@qq.com
 * @Date: 2025-09-09 17:03:48
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-29 23:30:36
 * @FilePath: /MAS_RM_DJIC/Mas_User/BSP/CAN/bsp_can_h7.c
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

#define CAN_BUS_NUM            3
#define CAN_DEVICE_VALID_CKECK 0xABCDEF01

// CAN总线管理器数组
static CAN_Bus_Manager can_bues[CAN_BUS_NUM];
// 全局CAN事件组
static TX_EVENT_FLAGS_GROUP can_event;
// 使用索引计数器
static uint16_t fdcan_filter_idx[3] = {0, 0, 0};
// 用于判断是否在定时器回调中
extern TX_THREAD _tx_timer_thread;

/**
 * @brief 添加 FDCAN 标准过滤器
 * @param hfdcan  FDCAN 句柄
 * @param rx_id   需要接收的标准 ID (11-bit)
 * @return        0: 成功, -1: 失败 (过滤器数量超限)
 */
int8_t FDCANConfigFilter(FDCAN_HandleTypeDef *hfdcan, uint32_t rx_id, uint8_t *out_bank_index)
{
    FDCAN_FilterTypeDef sFilterConfig;
    uint8_t             bus_idx = 0xFF;

    /* 确定总线 ID */
    if (hfdcan->Instance == FDCAN1)
        bus_idx = 0;
    else if (hfdcan->Instance == FDCAN2)
        bus_idx = 1;
    else if (hfdcan->Instance == FDCAN3)
        bus_idx = 2;
    else
        return -1;

    /*
     * 检查 FilterIndex 是否溢出，物理地址 = Base + Index * 4
     * 不能超出 CubeMX 中配置的 StdFiltersNbr 范围，否则会覆盖其他 RAM 区域
     */
    if (fdcan_filter_idx[bus_idx] >= hfdcan->Init.StdFiltersNbr)
    {
        return -1; // 索引用完
    }

    // 每次调用，索引递增，对应 RAM 地址后移 4 字节
    sFilterConfig.FilterIndex = fdcan_filter_idx[bus_idx];
    // IdType: 标准id标识符类型
    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    // FilterType: 选掩码模式
    sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    // FIFO 分流策略：奇数 ID 进 FIFO1，偶数 ID 进 FIFO0
    sFilterConfig.FilterConfig = (rx_id & 1) ? FDCAN_FILTER_TO_RXFIFO1 : FDCAN_FILTER_TO_RXFIFO0;
    /* FilterID1: 比较值 (要匹配的 ID) */
    sFilterConfig.FilterID1 = rx_id;
    /* FilterID2: 掩码 (Mask)，(RxID & Mask) == (TargetID & Mask)
     * 设置 Mask = 0x7FF (11位全1)，表示每一位都必须精确匹配 FilterID1
     */
    sFilterConfig.FilterID2 = 0x7FF;
    /* RxBufferIndex: 仅在使用 Rx Buffer 模式时有效，这里不用 */
    sFilterConfig.RxBufferIndex = 0;
    /* HAL 库内部会根据 FilterIndex 和 MessageRAMOffset 计算 RAM 物理地址并写入 */
    if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
    {
        return -1;
    }

    // 全局fdcan过滤器设置
    HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE,
                                 FDCAN_FILTER_REMOTE);

    if (out_bank_index)
        *out_bank_index = (uint8_t)sFilterConfig.FilterIndex;

    /* 更新索引，为下一个设备准备 */
    fdcan_filter_idx[bus_idx]++;

    return 0;
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

static CAN_Bus_Manager *BSP_CAN_Get_Bus_Manager(FDCAN_HandleTypeDef *hcan)
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
    if (FDCANConfigFilter(dev->hcan, dev->rx_id, &dev->filter_bank_index) != 0)
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
    HAL_FDCAN_ActivateNotification(
        config->hcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0);
    // 启动总线
    HAL_FDCAN_Start(config->hcan);

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

    FDCAN_FilterTypeDef sFilterConfig;
    sFilterConfig.IdType        = FDCAN_STANDARD_ID;
    sFilterConfig.FilterType    = FDCAN_FILTER_MASK;
    sFilterConfig.FilterIndex   = dev->filter_bank_index; // 使用保存的索引
    sFilterConfig.FilterConfig  = FDCAN_FILTER_DISABLE;   // 禁用该条目
    sFilterConfig.FilterID1     = 0;
    sFilterConfig.FilterID2     = 0;
    sFilterConfig.RxBufferIndex = 0;
    HAL_FDCAN_ConfigFilter(dev->hcan, &sFilterConfig);

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

    FDCAN_TxHeaderTypeDef TxHeader;
    TxHeader.Identifier          = device->tx_id;
    TxHeader.IdType              = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
    TxHeader.DataLength          = (len > 8) ? FDCAN_DLC_BYTES_8 : len;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker       = 0;

    // 对于发送，清理D-Cache
    uint32_t start = (uint32_t)data & ~0x1F;
    uint32_t end   = ((uint32_t)data + len + 31) & ~0x1F;
    SCB_CleanDCache_by_Addr((uint32_t *)start, end - start);

    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(device->hcan, &TxHeader, data);

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

    FDCAN_TxHeaderTypeDef TxHeader;
    TxHeader.Identifier          = cantxmeaasge->id;
    TxHeader.IdType              = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
    TxHeader.DataLength          = (cantxmeaasge->len > 8) ? FDCAN_DLC_BYTES_8 : cantxmeaasge->len;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker       = 0;

    // 对于发送，清理D-Cache
    uint32_t start = (uint32_t)cantxmeaasge->buf & ~0x1F;
    uint32_t end   = ((uint32_t)cantxmeaasge->buf + cantxmeaasge->len + 31) & ~0x1F;
    SCB_CleanDCache_by_Addr((uint32_t *)start, end - start);

    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(cantxmeaasge->hcan, &TxHeader, cantxmeaasge->buf);


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
 * @description: FDCAN 接收回调处理函数 (STM32H723)
 * @param {FDCAN_HandleTypeDef*} hfdcan 触发中断的 FDCAN 句柄
 * @param {uint32_t} RxFifo       接收 FIFO 编号 (FDCAN_RX_FIFO0 或 FDCAN_RX_FIFO1)
 */
static void BSP_FDCAN_RxCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo)
{
    // 查找对应的总线管理器
    CAN_Bus_Manager *bus_manager = BSP_CAN_Get_Bus_Manager(hfdcan);

    if (bus_manager == NULL)
    {
        return;
    }

    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t               rx_data[8];

    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, RxFifo) > 0)
    {
        if (HAL_FDCAN_GetRxMessage(hfdcan, RxFifo, &rx_header, rx_data) == HAL_OK)
        {
            Can_Device *current = bus_manager->device_list;
            while (current != NULL)
            {
                if (current->rx_id == rx_header.Identifier)
                {
                    // FDCAN 的 DataLength 字段存储的是 DLC 值。对于经典 CAN (<= 8
                    // bytes)，DataLength 低位即为字节数。
                    uint8_t len = (uint8_t)(rx_header.DataLength & 0x0F);
                    if (len > 8)
                        len = 8;

                    memcpy(current->rx_buf, rx_data, len);
                    current->real_rx_len = len;

                    tx_event_flags_set(&can_event, current->eventflag, TX_OR);
                    break;
                }
                current = current->next;
            }
        }
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
    {
        BSP_FDCAN_RxCallback(hfdcan, FDCAN_RX_FIFO0);
    }
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo1ITs)
{
    if ((RxFifo1ITs & FDCAN_IT_RX_FIFO1_NEW_MESSAGE) != 0)
    {
        BSP_FDCAN_RxCallback(hfdcan, FDCAN_RX_FIFO1);
    }
}
