/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 14:26:39
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-28 20:15:15
 * @FilePath: /MAS_RM_DJIC/Mas_User/BSP/UART/bsp_uart_h7.c
 * @Description:
 */
#include "bsp_def.h"
#include "bsp_uart.h"
#include "stm32h7xx_hal_uart.h"
#include "tx_api.h"
#include "tx_port.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LOG_TAG "bsp_uart"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define UART_DEVICE_VALID_CKECK 0xABCDEF01

// UART设备链表头指针
static UART_Device *uart_device_list = NULL; // 注意，这里无锁链表，只能在初始化期间操作

/* 内部函数声明 */
static void         Start_Rx(UART_Device *device);
static void         Process_Rx_Complete(UART_Device *device, uint16_t Size);
static UART_Device *BSP_UART_Find_Device(UART_HandleTypeDef *huart);

/**
 * @brief 启动UART接收
 */
static void Start_Rx(UART_Device *device)
{
    // 验证设备有效性
    if (device->valid != UART_DEVICE_VALID_CKECK)
    {
        return;
    }

    // 阻塞模式不需要启动后台接收
    if (device->rx_mode == UART_MODE_BLOCKING)
    {
        return;
    }

    uint8_t           next_buf = !device->rx_active_buf;
    HAL_StatusTypeDef status   = HAL_OK;

    // 先切换活动缓冲区，再启动DMA
    device->rx_active_buf = next_buf;

    // DMA模式下，启动接收前先清理并失效对应缓冲区的DCache
    if (device->rx_mode == UART_MODE_DMA)
    {
        uint32_t buf_addr = (uint32_t)device->rx_buf[next_buf];
        uint32_t start    = buf_addr & ~0x1F;                              // 32字节对齐向下取整
        uint32_t end      = (buf_addr + device->rx_buf_size + 31) & ~0x1F; // 32字节对齐向上取整
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)start, end - start);
    }

    // 根据接收模式启动相应的接收
    if (device->rx_mode == UART_MODE_IT)
    {
        status = HAL_UARTEx_ReceiveToIdle_IT(device->huart, device->rx_buf[next_buf],
                                             device->expected_rx_len ? device->expected_rx_len
                                                                     : device->rx_buf_size);
    }
    else if (device->rx_mode == UART_MODE_DMA)
    {
        status = HAL_UARTEx_ReceiveToIdle_DMA(device->huart, device->rx_buf[next_buf],
                                              device->expected_rx_len ? device->expected_rx_len
                                                                      : device->rx_buf_size);
    }

    // 如果启动失败，恢复原来的缓冲区标志
    if (status != HAL_OK)
    {
        device->rx_active_buf = !next_buf;
    }
}

/**
 * @brief 处理接收完成
 */
static void Process_Rx_Complete(UART_Device *device, uint16_t Size)
{
    // 验证设备有效性
    if (device->valid != UART_DEVICE_VALID_CKECK)
    {
        return;
    }

    // 长度校验
    if (Size > device->rx_buf_size)
    {
        Size = device->rx_buf_size;
    }

    // 如果设置了期望长度，且接收长度不匹配，则丢弃数据
    if (device->expected_rx_len > 0 && Size != device->expected_rx_len)
    {
        // 接收长度不符合预期，不通知应用层，直接丢弃
        return;
    }

    device->real_rx_len = Size;
    // 事件通知
    tx_event_flags_set(&device->uart_event, UART_RX_DONE_EVENT, TX_OR);
}

/**
 * @brief 根据UART句柄查找设备
 */
static UART_Device *BSP_UART_Find_Device(UART_HandleTypeDef *huart)
{
    UART_Device *current = uart_device_list;

    while (current != NULL)
    {
        if (current->huart == huart && current->valid == UART_DEVICE_VALID_CKECK)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

UART_Device *BSP_UART_Device_Init(UART_Device_init_config *config)
{
    // 参数检查
    if (!config || !config->huart)
    {
        log_e("Invalid config or huart is NULL");
        return NULL;
    }

    // 检查实例是否已存在
    UART_Device *existing_device = BSP_UART_Find_Device(config->huart);
    if (existing_device != NULL)
    {
        log_e("UART instance already exists, huart=%p", config->huart);
        return NULL;
    }

    // 分配设备结构体
    UART_Device *device;
    BSP_MEM_ALLOC_WAIT(device, sizeof(UART_Device), TX_NO_WAIT);
    if (device == NULL)
    {
        log_e("Failed to allocate UART device");
        return NULL;
    }

    // 初始化设备结构体
    memset(device, 0, sizeof(UART_Device));

    // 设置设备有效性魔数
    device->valid = UART_DEVICE_VALID_CKECK;

    // 配置UART句柄
    device->huart = config->huart;
    // 配置模式
    device->rx_mode = config->rx_mode;
    device->tx_mode = config->tx_mode;

    // 缓冲区配置部分
    if (config->rx_buf != NULL)
    {
        // 检查缓冲区基地址是否 32 字节对齐
        if (((uint32_t)config->rx_buf & 0x1F) != 0)
        {
            log_e("Error: RX Buffer address 0x%p must be 32-byte aligned!", config->rx_buf);
            BSP_MEM_FREE(device);
            return NULL;
        }
        // 检查缓冲区是否在DMA无法访问的内存区域
        // DTCMRAM: 0x20000000 - 0x2001FFFF (128KB)
        // ITCMRAM: 0x00000000 - 0x0000FFFF (64KB)
        uint32_t buf_addr = (uint32_t)config->rx_buf;
        if ((buf_addr >= 0x20000000 && buf_addr < 0x20020000) ||
            (buf_addr >= 0x00000000 && buf_addr < 0x00010000))
        {
            log_e("Error: RX Buffer address %p is in DMA inaccessible memory!", config->rx_buf);
            BSP_MEM_FREE(device);
            return NULL;
        }
        device->rx_buf      = (uint8_t (*)[2])config->rx_buf;
        device->rx_buf_size = config->rx_buf_size;
    }
    else
    {
        log_e("RX buffer is NULL");
        BSP_MEM_FREE(device);
        return NULL;
    }
    // 配置接收长度
    device->expected_rx_len = config->expected_rx_len;
    if (device->expected_rx_len > device->rx_buf_size)
    {
        device->expected_rx_len = device->rx_buf_size;
    } // 确保不超过缓冲区大小

    // 创建事件组
    UINT status = tx_event_flags_create(&device->uart_event, NULL);
    if (status != TX_SUCCESS)
    {
        log_e("Failed to create UART event, status=%d", status);
        BSP_MEM_FREE(device);
        return NULL;
    }

    // 添加到链表头部
    device->next     = uart_device_list;
    uart_device_list = device;

    Start_Rx(device);
    log_i("UART device initialized successfully, rx_mode=%d, tx_mode=%d", device->rx_mode,
          device->tx_mode);
    return device;
}

int BSP_UART_Send(UART_Device *device, uint8_t *data, uint16_t len, uint32_t timeout)
{
    if (device == NULL || data == NULL || len == 0)
    {
        log_e("Invalid parameters: device=%p, data=%p, len=%d", (void *)device, (void *)data, len);
        return -1;
    }

    // 验证设备有效性
    if (device->valid != UART_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return -1;
    }

    HAL_StatusTypeDef hal_status;

    // 根据发送模式选择发送方式
    switch (device->tx_mode)
    {
    case UART_MODE_BLOCKING:
    {
        unsigned int hal_timeout = (timeout == TX_WAIT_FOREVER) ? HAL_MAX_DELAY : timeout;
        hal_status               = HAL_UART_Transmit(device->huart, data, len, hal_timeout);
        break;
    }
    case UART_MODE_IT:
        hal_status = HAL_UART_Transmit_IT(device->huart, data, len);
        break;
    case UART_MODE_DMA:
    {
        // 检查缓冲区是否在DMA无法访问的内存区域
        // DTCMRAM: 0x20000000 - 0x2001FFFF (128KB)
        // ITCMRAM: 0x00000000 - 0x0000FFFF (64KB)
        uint32_t buf_addr = (uint32_t)data;
        if ((buf_addr >= 0x20000000 && buf_addr < 0x20020000) ||
            (buf_addr >= 0x00000000 && buf_addr < 0x00010000))
        {
            log_e("Error: TX Buffer address %p is in DMA inaccessible memory!", data);
            return -1;
        }
        // 32字节对齐
        uint32_t start = (uint32_t)data & ~0x1F;
        uint32_t end   = ((uint32_t)data + len + 31) & ~0x1F;
        SCB_CleanDCache_by_Addr((uint32_t *)start, end - start);

        hal_status = HAL_UART_Transmit_DMA(device->huart, data, len);
        break;
    }

    default:
        log_e("Invalid TX mode: %d", device->tx_mode);
        return -1;
    }

    // 对于非阻塞模式，等待发送完成事件
    if (hal_status == HAL_OK && device->tx_mode != UART_MODE_BLOCKING)
    {
        ULONG actual_flags;
        UINT status = tx_event_flags_get(&device->uart_event, UART_TX_DONE_EVENT | UART_ERROR_EVENT,
                                         TX_OR_CLEAR, &actual_flags, timeout);
        // 检查是否发生错误
        if (actual_flags & UART_ERROR_EVENT)
        {
            return -1;
        }
        return (status == TX_SUCCESS) ? len : -1;
    }

    // 对于阻塞模式，直接返回结果
    if (device->tx_mode == UART_MODE_BLOCKING)
    {
        return (hal_status == HAL_OK) ? len : -1;
    }

    return -1;
}

uint8_t *BSP_UART_Read(UART_Device *device, uint32_t timeout)
{
    if (device == NULL)
    {
        log_e("Invalid device for UART read");
        return NULL;
    }

    // 验证设备有效性
    if (device->valid != UART_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return NULL;
    }

    // 对于阻塞模式，使用第一个缓冲区进行接收
    if (device->rx_mode == UART_MODE_BLOCKING)
    {
        unsigned int      hal_timeout = (timeout == TX_WAIT_FOREVER) ? HAL_MAX_DELAY : timeout;
        HAL_StatusTypeDef status      = HAL_UARTEx_ReceiveToIdle(
            device->huart, (uint8_t *)device->rx_buf[0],
            device->expected_rx_len ? device->expected_rx_len : device->rx_buf_size,
            &device->real_rx_len, hal_timeout);
        if (status == HAL_OK)
        {
            return device->rx_buf[0];
        }
        return NULL;
    }
    else
    {
        // 对于中断/DMA模式，等待接收完成或错误事件
        ULONG actual_flags;
        UINT status = tx_event_flags_get(&device->uart_event, UART_RX_DONE_EVENT | UART_ERROR_EVENT,
                                         TX_OR_CLEAR, &actual_flags, timeout);
        if (status == TX_SUCCESS)
        {
            // 检查是否发生错误
            if (actual_flags & UART_ERROR_EVENT)
            {
                return NULL;
            }

            if (device->rx_mode == UART_MODE_DMA)
            {
                // 使cache失效。保证读取到最新数据（按32字节对齐）
                uint32_t buf_addr = (uint32_t)device->rx_buf[!device->rx_active_buf];
                uint32_t start    = buf_addr & ~0x1F;
                uint32_t end      = (buf_addr + device->rx_buf_size + 31) & ~0x1F;
                SCB_InvalidateDCache_by_Addr((uint32_t *)start, end - start);
            }
            // 返回非活动缓冲区的数据指针
            return device->rx_buf[!device->rx_active_buf];
        }
        return NULL;
    }
}

UINT BSP_UART_Deinit(UART_Device *device)
{
    if (device == NULL)
    {
        log_e("Invalid device for UART deinit");
        return TX_DELETED;
    }

    // 验证设备有效性
    if (device->valid != UART_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    // 从链表中查找并移除节点
    UART_Device *prev    = NULL;
    UART_Device *current = uart_device_list;
    bool         found   = false;

    while (current != NULL)
    {
        if (current == device)
        {
            // 找到节点，从链表中移除
            if (prev == NULL)
            {
                // 移除的是头节点
                uart_device_list = current->next;
            }
            else
            {
                // 移除的是中间或尾部节点
                prev->next = current->next;
            }
            found = true;
            break;
        }
        prev    = current;
        current = current->next;
    }

    if (!found)
    {
        log_e("Device not found in list");
        return TX_DELETED;
    }

    // 终止UART传输
    HAL_UART_Abort(device->huart);

    // 删除事件标志组
    tx_event_flags_delete(&device->uart_event);

    // 释放内存
    BSP_MEM_FREE(device);

    log_i("UART device deinitialized successfully, huart=%p", device->huart);
    return TX_SUCCESS;
}

/* 中断函数 */

// 接收完成回调
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    UART_Device *device           = BSP_UART_Find_Device(huart);
    if (device != NULL && device->valid == UART_DEVICE_VALID_CKECK)
    {
        Process_Rx_Complete(device, Size);
        Start_Rx(device); // 重启接收
    }
}

// 发送完成回调
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    UART_Device *device = BSP_UART_Find_Device(huart);
    if (device != NULL && device->valid == UART_DEVICE_VALID_CKECK)
    {
        tx_event_flags_set(&device->uart_event, UART_TX_DONE_EVENT, TX_OR);
    }
}

// 错误处理回调
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    UART_Device *device = BSP_UART_Find_Device(huart);
    if (device != NULL && device->valid == UART_DEVICE_VALID_CKECK)
    {
        // 清零接收长度，防止读取到无效数据
        device->real_rx_len = 0;

        // 中止UART传输
        HAL_UART_Abort(huart);

        // 重启接收
        Start_Rx(device);

        // 设置错误事件标志，通知等待线程
        tx_event_flags_set(&device->uart_event, UART_ERROR_EVENT, TX_OR);
    }
}
