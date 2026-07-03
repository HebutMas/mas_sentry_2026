/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-08 10:44:23
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-19 13:44:53
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/BSP/I2C/bsp_i2c_f4.c
 * @Description:
 */
#include "bsp_def.h"
#include "bsp_i2c.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdio.h>

#define LOG_TAG "bsp_i2c"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define I2C_DEVICE_VALID_CKECK 0xABCDEF01

static I2C_Bus_Manager i2c_buses[I2C_BUS_NUM] = {0};

// 用于判断是否在定时器回调中
extern TX_THREAD _tx_timer_thread;

/* I2C传输类型定义 */
typedef enum
{
    I2C_OP_TYPE_TRANSMIT,
    I2C_OP_TYPE_RECEIVE,
    I2C_OP_TYPE_MEM_TRANSMIT,
    I2C_OP_TYPE_MEM_RECEIVE
} I2C_Operation_Type;

// 内部函数声明;
static I2C_Bus_Manager *BSP_I2C_Get_Bus_Manager(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL)
    {
        log_e("NULL hi2c parameter");
        return NULL;
    }

    for (int i = 0; i < I2C_BUS_NUM; i++)
    {
        if (i2c_buses[i].hi2c == hi2c || i2c_buses[i].hi2c == NULL)
        {
            return &i2c_buses[i];
        }
    }
    return NULL;
}

static I2C_Bus_Manager *BSP_I2C_Pre_Transfer_Init(I2C_Device *dev, uint32_t timeout)
{
    // 验证设备有效性
    if (dev->valid != I2C_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return NULL;
    }

    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(dev->hi2c);
    if (bus_manager == NULL)
    {
        log_e("Device bus_manager is NULL");
        return NULL;
    }

    TX_THREAD *current_thread = tx_thread_identify();

    // 判断是否在初始化阶段,不使用互斥量
    if (current_thread != NULL && current_thread != &_tx_timer_thread)
    {
        UINT status = tx_mutex_get(&bus_manager->bus_mutex, timeout);
        if (status != TX_SUCCESS)
        {
            log_e("Failed to acquire bus mutex");
            return NULL;
        }
    }

    // 检查I2C状态
    if (HAL_I2C_GetState(dev->hi2c) != HAL_I2C_STATE_READY)
    {
        // 判断是否在初始化阶段,不使用互斥量
        if (current_thread != NULL && current_thread != &_tx_timer_thread)
        {
            tx_mutex_put(&bus_manager->bus_mutex);
        }
        return NULL;
    }

    return bus_manager;
}

static void BSP_I2C_Post_Transfer_Cleanup(I2C_Bus_Manager *bus_manager)
{
    TX_THREAD *current_thread = tx_thread_identify();
    // 判断是否在初始化阶段,不使用互斥量
    if (current_thread  != NULL && current_thread != &_tx_timer_thread)
    {
        tx_mutex_put(&bus_manager->bus_mutex);
    }
}

static UINT BSP_I2C_Common_Transfer(I2C_Device *dev, I2C_Operation_Type op_type, uint8_t *tx_data,
                                    uint8_t *rx_data, uint16_t size, uint16_t mem_address,
                                    uint16_t mem_add_size, uint32_t timeout)
{
    HAL_StatusTypeDef hal_status;
    UINT              status      = TX_SUCCESS;
    I2C_Bus_Manager  *bus_manager = BSP_I2C_Get_Bus_Manager(dev->hi2c);
    I2C_Mode          mode;
    const char       *op_name;
    uint32_t          event_flag;

    // 根据操作类型设置参数
    switch (op_type)
    {
    case I2C_OP_TYPE_TRANSMIT:
    case I2C_OP_TYPE_MEM_TRANSMIT:
        mode       = dev->tx_mode;
        op_name    = "Transmit";
        event_flag = I2C_EVENT_TX_COMPLETE;
        break;
    case I2C_OP_TYPE_RECEIVE:
    case I2C_OP_TYPE_MEM_RECEIVE:
        mode       = dev->rx_mode;
        op_name    = "Receive";
        event_flag = I2C_EVENT_RX_COMPLETE;
        break;
    default:
        return TX_DELETED;
    }

    TX_THREAD *current_thread = tx_thread_identify();

    // 判断是否在初始化阶段,初始化期间强制性使用blocking函数
    if (current_thread == NULL || current_thread == &_tx_timer_thread)
    {
        if (op_type == I2C_OP_TYPE_TRANSMIT)
        {
            hal_status =
                HAL_I2C_Master_Transmit(dev->hi2c, dev->dev_address, tx_data, size, HAL_MAX_DELAY);
        }
        else if (op_type == I2C_OP_TYPE_RECEIVE)
        {
            hal_status =
                HAL_I2C_Master_Receive(dev->hi2c, dev->dev_address, rx_data, size, HAL_MAX_DELAY);
        }
        else if (op_type == I2C_OP_TYPE_MEM_TRANSMIT)
        {
            hal_status = HAL_I2C_Mem_Write(dev->hi2c, dev->dev_address, mem_address, mem_add_size,
                                           tx_data, size, HAL_MAX_DELAY);
        }
        else if (op_type == I2C_OP_TYPE_MEM_RECEIVE)
        {
            hal_status = HAL_I2C_Mem_Read(dev->hi2c, dev->dev_address, mem_address, mem_add_size,
                                          rx_data, size, HAL_MAX_DELAY);
        }

        if (hal_status != HAL_OK)
        {
            log_e("HAL I2C %s failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }

        return status;
    }

    // 根据模式执行传输
    switch (mode)
    {
    case I2C_MODE_BLOCKING:
    {
        if (op_type == I2C_OP_TYPE_TRANSMIT)
        {
            hal_status =
                HAL_I2C_Master_Transmit(dev->hi2c, dev->dev_address, tx_data, size, timeout);
        }
        else if (op_type == I2C_OP_TYPE_RECEIVE)
        {
            hal_status =
                HAL_I2C_Master_Receive(dev->hi2c, dev->dev_address, rx_data, size, timeout);
        }
        else if (op_type == I2C_OP_TYPE_MEM_TRANSMIT)
        {
            hal_status = HAL_I2C_Mem_Write(dev->hi2c, dev->dev_address, mem_address, mem_add_size,
                                           tx_data, size, timeout);
        }
        else if (op_type == I2C_OP_TYPE_MEM_RECEIVE)
        {
            hal_status = HAL_I2C_Mem_Read(dev->hi2c, dev->dev_address, mem_address, mem_add_size,
                                          rx_data, size, timeout);
        }

        if (hal_status != HAL_OK)
        {
            log_e("HAL I2C %s failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        break;
    }
    case I2C_MODE_IT:
    {
        if (op_type == I2C_OP_TYPE_TRANSMIT)
            hal_status = HAL_I2C_Master_Transmit_IT(dev->hi2c, dev->dev_address, tx_data, size);
        else if (op_type == I2C_OP_TYPE_RECEIVE)
            hal_status = HAL_I2C_Master_Receive_IT(dev->hi2c, dev->dev_address, rx_data, size);
        else if (op_type == I2C_OP_TYPE_MEM_TRANSMIT)
            hal_status = HAL_I2C_Mem_Write_IT(dev->hi2c, dev->dev_address, mem_address,
                                              mem_add_size, tx_data, size);
        else if (op_type == I2C_OP_TYPE_MEM_RECEIVE)
            hal_status = HAL_I2C_Mem_Read_IT(dev->hi2c, dev->dev_address, mem_address, mem_add_size,
                                             rx_data, size);

        if (hal_status == HAL_OK)
        {
            ULONG actual_flags;
            status = tx_event_flags_get(&bus_manager->bus_event, event_flag | I2C_EVENT_ERROR,
                                        TX_OR_CLEAR, &actual_flags, timeout);
            if (status != TX_SUCCESS)
            {
                log_e("Failed to wait for I2C event");
                status = TX_DELETED;
            }
            else if (actual_flags & I2C_EVENT_ERROR)
            {
                log_e("I2C error occurred during IT %s", op_name);
                status = TX_DELETED;
            }
        }
        else
        {
            log_e("HAL I2C %s_IT failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        break;
    }

    case I2C_MODE_DMA:
    {
        if (op_type == I2C_OP_TYPE_TRANSMIT)
            hal_status = HAL_I2C_Master_Transmit_DMA(dev->hi2c, dev->dev_address, tx_data, size);
        else if (op_type == I2C_OP_TYPE_RECEIVE)
            hal_status = HAL_I2C_Master_Receive_DMA(dev->hi2c, dev->dev_address, rx_data, size);
        else if (op_type == I2C_OP_TYPE_MEM_TRANSMIT)
            hal_status = HAL_I2C_Mem_Write_DMA(dev->hi2c, dev->dev_address, mem_address,
                                               mem_add_size, tx_data, size);
        else if (op_type == I2C_OP_TYPE_MEM_RECEIVE)
            hal_status = HAL_I2C_Mem_Read_DMA(dev->hi2c, dev->dev_address, mem_address,
                                              mem_add_size, rx_data, size);

        if (hal_status == HAL_OK)
        {
            // 等待 DMA 传输完成
            ULONG actual_flags;
            status = tx_event_flags_get(&bus_manager->bus_event, event_flag | I2C_EVENT_ERROR,
                                        TX_OR_CLEAR, &actual_flags, timeout);
            if (status != TX_SUCCESS)
            {
                log_e("Failed to wait for I2C DMA event");
                status = TX_DELETED;
            }
            else if (actual_flags & I2C_EVENT_ERROR)
            {
                log_e("I2C error occurred during DMA %s", op_name);
                status = TX_DELETED;
            }
        }
        else
        {
            log_e("HAL I2C %s_DMA failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        break;
    }
    default:
        log_e("Invalid I2C mode: %d", mode);
        status = TX_DELETED;
        break;
    }

    return status;
}

I2C_Device *BSP_I2C_Device_Init(I2C_Device_Init_Config *config)
{
    if (config == NULL || config->hi2c == NULL)
    {
        log_e("Invalid config or hi2c is NULL");
        return NULL;
    }

    // 获取对应的总线管理器
    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(config->hi2c);
    if (bus_manager == NULL)
    {
        log_e("Failed to get bus manager for hi2c=%p", (void *)config->hi2c);
        return NULL;
    }

    // 初始化总线管理器（如果尚未初始化）
    if (bus_manager->hi2c == NULL)
    {
        bus_manager->hi2c        = config->hi2c;
        bus_manager->device_list = NULL;

        // 创建总线信号量
        if (tx_mutex_create(&bus_manager->bus_mutex, NULL, TX_INHERIT) != TX_SUCCESS)
        {
            log_e("Failed to create bus mutex");
            bus_manager->hi2c = NULL;
            return NULL;
        }

        // 创建总线事件
        if (tx_event_flags_create(&bus_manager->bus_event, NULL) != TX_SUCCESS)
        {
            log_e("Failed to create bus event");
            bus_manager->hi2c = NULL;
            return NULL;
        }
    }

    // 分配新设备内存
    I2C_Device *dev = NULL;
    BSP_MEM_ALLOC_WAIT(dev, sizeof(I2C_Device), TX_NO_WAIT);

    if (dev == NULL)
    {
        log_e("Failed to allocate memory for I2C device");
        return NULL;
    }

    // 初始化设备
    dev->valid       = I2C_DEVICE_VALID_CKECK;
    dev->hi2c        = config->hi2c;
    dev->dev_address = config->dev_address;
    dev->tx_mode     = config->tx_mode;
    dev->rx_mode     = config->rx_mode;
    dev->next        = NULL;

    // 将设备添加到链表头部
    dev->next                = bus_manager->device_list;
    bus_manager->device_list = dev;

    log_i("I2C device initialized successfully");

    return dev;
}

UINT BSP_I2C_Device_DeInit(I2C_Device *dev)
{
    if (dev == NULL)
    {
        return TX_DELETED;
    }

    // 验证设备有效性
    if (dev->valid != I2C_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(dev->hi2c);
    if (bus_manager == NULL)
    {
        log_i("Failed to get bus manager for device hi2c=%p", (void *)dev->hi2c);
        return TX_DELETED;
    }

    // 获取总线信号量
    UINT status = tx_mutex_get(&bus_manager->bus_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS)
    {
        log_e("Failed to acquire bus mutex");
        return TX_DELETED;
    }

    // 从链表中移除设备
    if (bus_manager->device_list == dev)
    {
        // 设备是头节点
        bus_manager->device_list = dev->next;
    }
    else
    {
        // 查找设备的前一个节点
        I2C_Device *current = bus_manager->device_list;
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
        tx_mutex_delete(&bus_manager->bus_mutex);
        tx_event_flags_delete(&bus_manager->bus_event);
        bus_manager->hi2c = NULL;
    }

    log_i("i2c device deinitialized successfully");

    return TX_SUCCESS;
}

UINT BSP_I2C_Transmit(I2C_Device *dev, uint8_t *tx_data, uint16_t size, uint32_t timeout)
{
    if (dev == NULL || tx_data == NULL || size == 0)
    {
        log_e("Invalid parameters: dev=%p, tx_data=%p, size=%d", (void *)dev, (void *)tx_data,
              size);
        return TX_DELETED;
    }

    I2C_Bus_Manager *bus_manager = BSP_I2C_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status =
        BSP_I2C_Common_Transfer(dev, I2C_OP_TYPE_TRANSMIT, tx_data, NULL, size, 0, 0, timeout);

    BSP_I2C_Post_Transfer_Cleanup(bus_manager);

    return status;
}

UINT BSP_I2C_Receive(I2C_Device *dev, uint8_t *rx_data, uint16_t size, uint32_t timeout)
{
    if (dev == NULL || rx_data == NULL || size == 0)
    {
        log_e("Invalid parameters: dev=%p, rx_data=%p, size=%d", (void *)dev, (void *)rx_data,
              size);
        return TX_DELETED;
    }

    I2C_Bus_Manager *bus_manager = BSP_I2C_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status =
        BSP_I2C_Common_Transfer(dev, I2C_OP_TYPE_RECEIVE, NULL, rx_data, size, 0, 0, timeout);

    BSP_I2C_Post_Transfer_Cleanup(bus_manager);

    return status;
}

UINT BSP_I2C_Mem_Write(I2C_Device *dev, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data,
                       uint16_t size, uint32_t timeout)
{
    if (dev == NULL || data == NULL || size == 0)
    {
        log_e("Invalid parameters: dev=%p, data=%p, size=%d", (void *)dev, (void *)data, size);
        return TX_DELETED;
    }

    I2C_Bus_Manager *bus_manager = BSP_I2C_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status = BSP_I2C_Common_Transfer(dev, I2C_OP_TYPE_MEM_TRANSMIT, data, NULL, size,
                                          mem_address, mem_add_size, timeout);

    BSP_I2C_Post_Transfer_Cleanup(bus_manager);

    return status;
}

UINT BSP_I2C_Mem_Read(I2C_Device *dev, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data,
                      uint16_t size, uint32_t timeout)
{
    if (dev == NULL || data == NULL || size == 0)
    {
        log_e("Invalid parameters: dev=%p, data=%p, size=%d", (void *)dev, (void *)data, size);
        return TX_DELETED;
    }

    I2C_Bus_Manager *bus_manager = BSP_I2C_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status = BSP_I2C_Common_Transfer(dev, I2C_OP_TYPE_MEM_RECEIVE, NULL, data, size,
                                          mem_address, mem_add_size, timeout);

    BSP_I2C_Post_Transfer_Cleanup(bus_manager);

    return status;
}

/* 中断回调函数 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(hi2c);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, I2C_EVENT_TX_COMPLETE, TX_OR);
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(hi2c);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, I2C_EVENT_RX_COMPLETE, TX_OR);
    }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(hi2c);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, I2C_EVENT_TX_COMPLETE, TX_OR);
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(hi2c);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, I2C_EVENT_RX_COMPLETE, TX_OR);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Bus_Manager *bus_manager = BSP_I2C_Get_Bus_Manager(hi2c);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, I2C_EVENT_ERROR, TX_OR);
    }
}