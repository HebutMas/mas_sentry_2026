#include "bsp_spi.h"
#include <stdio.h>

#define LOG_TAG "bsp_spi"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define SPI_DEVICE_VALID_CKECK 0xABCDEF01

// 用于判断是否在定时器回调中
extern TX_THREAD _tx_timer_thread;

// SPI总线管理器数组
static SPI_Bus_Manager spi_buses[SPI_BUS_NUM];

/* SPI传输类型定义 */
typedef enum
{
    SPI_OP_TYPE_TRANSMIT,
    SPI_OP_TYPE_RECEIVE,
    SPI_OP_TYPE_TRANSMIT_RECEIVE
} SPI_Operation_Type;

/* 内部函数声明 */
static UINT BSP_SPI_Check_DMA_Buffer(const uint8_t *buffer, uint16_t size)
{
    if (buffer == NULL)
    {
        return TX_DELETED;
    }

    uint32_t buf_addr = (uint32_t)buffer;

    // 检查缓冲区是否在DMA无法访问的内存区域
    // DTCMRAM: 0x20000000 - 0x2001FFFF (128KB) - DMA无法访问
    // ITCMRAM: 0x00000000 - 0x0000FFFF (64KB) - DMA无法访问
    if ((buf_addr >= 0x20000000 && buf_addr < 0x20020000) ||
        (buf_addr >= 0x00000000 && buf_addr < 0x00010000))
    {
        log_e("Error: Buffer address 0x%p is in DMA inaccessible memory (DTCMRAM/ITCMRAM)!",
              buffer);
        return TX_DELETED;
    }

    return TX_SUCCESS;
}

static SPI_Bus_Manager *BSP_SPI_Get_Bus_Manager(SPI_HandleTypeDef *hspi)
{
    if (hspi == NULL)
    {
        log_e("NULL hspi parameter");
        return NULL;
    }

    for (int i = 0; i < SPI_BUS_NUM; i++)
    {
        if (spi_buses[i].hspi == hspi || spi_buses[i].hspi == NULL)
        {
            return &spi_buses[i];
        }
    }

    return NULL;
}

static void BSP_SPI_Select_Device(SPI_Device *dev)
{
    if (dev != NULL && dev->cs_port != NULL && dev->cs_pin != 0)
    {
        HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
    }
}

static void BSP_SPI_Deselect_Device(SPI_Device *dev)
{
    if (dev != NULL && dev->cs_port != NULL && dev->cs_pin != 0)
    {
        HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
    }
}

static UINT BSP_SPI_Common_Transfer(SPI_Device *dev, SPI_Operation_Type op_type,
                                    const uint8_t *tx_data, uint8_t *rx_data, uint16_t size,
                                    uint32_t timeout)
{
    HAL_StatusTypeDef hal_status;
    UINT              status      = TX_SUCCESS;
    SPI_Bus_Manager  *bus_manager = BSP_SPI_Get_Bus_Manager(dev->hspi);
    SPI_Mode          mode;
    const char       *op_name;
    uint32_t          event_flag;

    // 根据操作类型设置参数
    switch (op_type)
    {
    case SPI_OP_TYPE_TRANSMIT:
        mode       = dev->tx_mode;
        op_name    = "Transmit";
        event_flag = SPI_EVENT_TX_DONE_EVENT;
        break;
    case SPI_OP_TYPE_RECEIVE:
        mode       = dev->rx_mode;
        op_name    = "Receive";
        event_flag = SPI_EVENT_RX_DONE_EVENT;
        break;
    case SPI_OP_TYPE_TRANSMIT_RECEIVE:
        mode       = dev->tx_mode;
        op_name    = "TransmitReceive";
        event_flag = SPI_EVENT_TX_RX_DONE_EVENT;
        break;
    default:
        log_e("Invalid operation type: %d", op_type);
        return TX_DELETED;
    }

    TX_THREAD *current_thread = tx_thread_identify();

    // 判断是否在初始化阶段,初始化期间强制性使用blocking函数
    if (current_thread == NULL || current_thread == &_tx_timer_thread)
    {
        if (op_type == SPI_OP_TYPE_TRANSMIT)
        {
            hal_status = HAL_SPI_Transmit(dev->hspi, tx_data, size, HAL_MAX_DELAY);
        }
        else if (op_type == SPI_OP_TYPE_RECEIVE)
        {
            hal_status = HAL_SPI_Receive(dev->hspi, rx_data, size, HAL_MAX_DELAY);
        }
        else if (op_type == SPI_OP_TYPE_TRANSMIT_RECEIVE)
        {
            hal_status = HAL_SPI_TransmitReceive(dev->hspi, tx_data, rx_data, size, HAL_MAX_DELAY);
        }

        if (hal_status != HAL_OK)
        {
            log_e("HAL SPI %s failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        return status;
    }

    // 根据模式执行传输
    switch (mode)
    {
    case SPI_MODE_BLOCKING:
    {
        if (op_type == SPI_OP_TYPE_TRANSMIT)
        {
            hal_status = HAL_SPI_Transmit(dev->hspi, tx_data, size, timeout);
        }
        else if (op_type == SPI_OP_TYPE_RECEIVE)
        {
            hal_status = HAL_SPI_Receive(dev->hspi, rx_data, size, timeout);
        }
        else if (op_type == SPI_OP_TYPE_TRANSMIT_RECEIVE)
        {
            hal_status = HAL_SPI_TransmitReceive(dev->hspi, tx_data, rx_data, size, timeout);
        }

        if (hal_status != HAL_OK)
        {
            log_e("HAL SPI %s failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        break;
    }
    case SPI_MODE_IT:
        if (op_type == SPI_OP_TYPE_TRANSMIT)
        {
            hal_status = HAL_SPI_Transmit_IT(dev->hspi, tx_data, size);
        }
        else if (op_type == SPI_OP_TYPE_RECEIVE)
        {
            hal_status = HAL_SPI_Receive_IT(dev->hspi, rx_data, size);
        }
        else if (op_type == SPI_OP_TYPE_TRANSMIT_RECEIVE)
        {
            hal_status = HAL_SPI_TransmitReceive_IT(dev->hspi, tx_data, rx_data, size);
        }

        if (hal_status == HAL_OK)
        {
            // 等待传输完成或出错
            ULONG actual_flags;
            status = tx_event_flags_get(&bus_manager->bus_event, event_flag | SPI_EVENT_ERROR_EVENT,
                                        TX_OR_CLEAR, &actual_flags, timeout);
            if (status != TX_SUCCESS)
            {
                log_e("Failed to wait for SPI event");
                status = TX_DELETED;
            }
            else if (actual_flags & SPI_EVENT_ERROR_EVENT)
            {
                log_e("SPI error occurred during IT %s", op_name);
                status = TX_DELETED;
            }
        }
        else
        {
            log_e("HAL SPI %s_IT failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        break;

    case SPI_MODE_DMA:
        // 检查DMA缓冲区是否可用
        if ((tx_data != NULL && BSP_SPI_Check_DMA_Buffer(tx_data, size) != TX_SUCCESS) ||
            (rx_data != NULL && BSP_SPI_Check_DMA_Buffer(rx_data, size) != TX_SUCCESS))
        {
            log_e("DMA buffer check failed");
            status = TX_DELETED;
            return status;
        }
        // 对于发送和收发，清理D-Cache
        if (op_type == SPI_OP_TYPE_TRANSMIT || op_type == SPI_OP_TYPE_TRANSMIT_RECEIVE)
        {
            uint32_t start = (uint32_t)tx_data & ~0x1F;
            uint32_t end   = ((uint32_t)tx_data + size + 31) & ~0x1F;
            SCB_CleanDCache_by_Addr((uint32_t *)start, end - start);
        }
        // 根据操作类型调用不同的HAL函数
        if (op_type == SPI_OP_TYPE_TRANSMIT)
        {
            hal_status = HAL_SPI_Transmit_DMA(dev->hspi, tx_data, size);
        }
        else if (op_type == SPI_OP_TYPE_RECEIVE)
        {
            hal_status = HAL_SPI_Receive_DMA(dev->hspi, rx_data, size);
        }
        else if (op_type == SPI_OP_TYPE_TRANSMIT_RECEIVE)
        {
            hal_status = HAL_SPI_TransmitReceive_DMA(dev->hspi, tx_data, rx_data, size);
        }

        if (hal_status == HAL_OK)
        {
            // 等待传输完成或出错
            ULONG actual_flags;
            status = tx_event_flags_get(&bus_manager->bus_event, event_flag | SPI_EVENT_ERROR_EVENT,
                                        TX_OR_CLEAR, &actual_flags, timeout);
            if (status != TX_SUCCESS)
            {
                log_e("Failed to wait for SPI event");
                status = TX_DELETED;
            }
            else if (actual_flags & SPI_EVENT_ERROR_EVENT)
            {
                log_e("SPI error occurred during DMA %s", op_name);
                status = TX_DELETED;
            }
            else
            {
                // 对于接收和收发，使D-Cache失效
                if (op_type == SPI_OP_TYPE_RECEIVE || op_type == SPI_OP_TYPE_TRANSMIT_RECEIVE)
                {
                    SCB_InvalidateDCache_by_Addr((uint32_t *)rx_data, size);
                }
            }
        }
        else
        {
            log_e("HAL SPI %s_DMA failed: %d", op_name, hal_status);
            status = TX_DELETED;
        }
        break;

    default:
        log_e("Invalid SPI mode: %d", mode);
        status = TX_DELETED;
        break;
    }

    return status;
}

static SPI_Bus_Manager *BSP_SPI_Pre_Transfer_Init(SPI_Device *dev, uint32_t timeout)
{
    // 验证设备有效性
    if (dev->valid != SPI_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return NULL;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(dev->hspi);
    if (bus_manager == NULL)
    {
        log_e("Device bus_manager is NULL");
        return NULL;
    }

    TX_THREAD *current_thread = tx_thread_identify();

    if (current_thread != NULL && current_thread != &_tx_timer_thread)
    {
        UINT status = tx_mutex_get(&bus_manager->bus_mutex, timeout);
        if (status != TX_SUCCESS)
        {
            log_e("Failed to acquire bus mutex");
            return NULL;
        }
    }

    // 检查SPI状态
    if (HAL_SPI_GetState(dev->hspi) != HAL_SPI_STATE_READY)
    {
        if (current_thread != NULL && current_thread != &_tx_timer_thread)
        {
            tx_mutex_put(&bus_manager->bus_mutex);
        }
        return NULL;
    }

    // 选中设备
    BSP_SPI_Select_Device(dev);

    return bus_manager;
}

static void BSP_SPI_Post_Transfer_Cleanup(SPI_Bus_Manager *bus_manager, SPI_Device *dev)
{
    // 取消选中设备
    BSP_SPI_Deselect_Device(dev);

    TX_THREAD *current_thread = tx_thread_identify();

    // 判断是否在初始化阶段,初始化期间强制性使用blocking函数
    if (current_thread != NULL && current_thread != &_tx_timer_thread)
    {
        tx_mutex_put(&bus_manager->bus_mutex);
    }
}

/* 对外接口函数 */

SPI_Device *BSP_SPI_Device_Init(SPI_Device_Init_Config *config)
{
    if (config == NULL || config->hspi == NULL)
    {
        log_e("Invalid config or hspi is NULL");
        return NULL;
    }

    // 获取对应的总线管理器
    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(config->hspi);
    if (bus_manager == NULL)
    {
        log_e("Failed to get bus manager for hspi=%p", (void *)config->hspi);
        return NULL;
    }

    // 初始化总线管理器（如果尚未初始化）
    if (bus_manager->hspi == NULL)
    {
        bus_manager->hspi        = config->hspi;
        bus_manager->device_list = NULL;

        // 创建总线信号量
        if (tx_mutex_create(&bus_manager->bus_mutex, NULL, TX_INHERIT) != TX_SUCCESS)
        {
            log_e("Failed to create bus mutex");
            bus_manager->hspi = NULL;
            return NULL;
        }

        // 创建总线事件
        if (tx_event_flags_create(&bus_manager->bus_event, NULL) != TX_SUCCESS)
        {
            log_e("Failed to create bus event");
            bus_manager->hspi = NULL;
            return NULL;
        }
    }

    // 分配新设备内存
    SPI_Device *dev = NULL;
    BSP_MEM_ALLOC_WAIT(dev, sizeof(SPI_Device), TX_NO_WAIT);

    if (dev == NULL)
    {
        log_e("Failed to allocate memory for SPI device");
        return NULL;
    }

    // 初始化设备
    dev->valid   = SPI_DEVICE_VALID_CKECK;
    dev->hspi    = config->hspi;
    dev->cs_port = config->cs_port;
    dev->cs_pin  = config->cs_pin;
    dev->tx_mode = config->tx_mode;
    dev->rx_mode = config->rx_mode;
    dev->next    = NULL;

    // 初始化时拉高CS引脚
    BSP_SPI_Deselect_Device(dev);

    // 将设备添加到链表头部
    dev->next                = bus_manager->device_list;
    bus_manager->device_list = dev;

    log_i("SPI device initialized successfully");

    return dev;
}

UINT BSP_SPI_Device_DeInit(SPI_Device *dev)
{
    if (dev == NULL)
    {
        log_e("Device pointer is NULL");
        return TX_DELETED;
    }

    // 验证设备有效性
    if (dev->valid != SPI_DEVICE_VALID_CKECK)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(dev->hspi);
    if (bus_manager == NULL)
    {
        log_e("Device bus_manager is NULL");
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
        SPI_Device *current = bus_manager->device_list;
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
        bus_manager->hspi = NULL;
    }

    log_i("SPI device deinitialized successfully");
    return TX_SUCCESS;
}

UINT BSP_SPI_TransReceive(SPI_Device *dev, const uint8_t *tx_data, uint8_t *rx_data, uint16_t size,
                          uint32_t timeout)
{
    if (dev == NULL || tx_data == NULL || rx_data == NULL || size == 0)
    {
        log_e("Invalid parameters: dev=%p, tx_data=%p, rx_data=%p, size=%d", (void *)dev,
              (void *)tx_data, (void *)rx_data, size);
        return TX_DELETED;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status =
        BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_TRANSMIT_RECEIVE, tx_data, rx_data, size, timeout);

    BSP_SPI_Post_Transfer_Cleanup(bus_manager, dev);

    return status;
}

UINT BSP_SPI_Transmit(SPI_Device *dev, const uint8_t *tx_data, uint16_t size, uint32_t timeout)
{
    if (dev == NULL || tx_data == NULL)
    {
        log_e("Invalid parameters: dev=%p, tx_data=%p, size=%d", (void *)dev, (void *)tx_data,
              size);
        return TX_DELETED;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status = BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_TRANSMIT, tx_data, NULL, size, timeout);

    BSP_SPI_Post_Transfer_Cleanup(bus_manager, dev);

    return status;
}

UINT BSP_SPI_Receive(SPI_Device *dev, uint8_t *rx_data, uint16_t size, uint32_t timeout)
{
    if (dev == NULL || rx_data == NULL || size == 0)
    {
        log_e("Invalid parameters: dev=%p, rx_data=%p, size=%d", (void *)dev, (void *)rx_data,
              size);
        return TX_DELETED;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status = BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_RECEIVE, NULL, rx_data, size, timeout);

    BSP_SPI_Post_Transfer_Cleanup(bus_manager, dev);

    return status;
}

UINT BSP_SPI_TransAndTrans(SPI_Device *dev, const uint8_t *tx_data1, uint16_t size1,
                           const uint8_t *tx_data2, uint16_t size2, uint32_t timeout)
{
    if (dev == NULL || (size1 > 0 && tx_data1 == NULL) || (size2 > 0 && tx_data2 == NULL) ||
        (size1 == 0 && size2 == 0))
    {
        log_e("Invalid parameters: dev=%p, tx_data1=%p, tx_data2=%p, size1=%d, size2=%d",
              (void *)dev, (void *)tx_data1, (void *)tx_data2, size1, size2);
        return TX_DELETED;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status = TX_SUCCESS;

    // 发送第一次数据
    if (size1 > 0)
    {
        status = BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_TRANSMIT, tx_data1, NULL, size1, timeout);
    }

    // 发送第二次数据（仅当第一次成功时）
    if (size2 > 0 && status == TX_SUCCESS)
    {
        status = BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_TRANSMIT, tx_data2, NULL, size2, timeout);
    }

    BSP_SPI_Post_Transfer_Cleanup(bus_manager, dev);

    return status;
}

UINT BSP_SPI_ReceAndRece(SPI_Device *dev, uint8_t *rx_data1, uint16_t size1, uint8_t *rx_data2,
                         uint16_t size2, uint32_t timeout)
{
    if (dev == NULL || (size1 > 0 && rx_data1 == NULL) || (size2 > 0 && rx_data2 == NULL) ||
        (size1 == 0 && size2 == 0))
    {
        log_e("Invalid parameters: dev=%p, rx_data1=%p, rx_data2=%p, size1=%d, size2=%d",
              (void *)dev, (void *)rx_data1, (void *)rx_data2, size1, size2);
        return TX_DELETED;
    }

    SPI_Bus_Manager *bus_manager = BSP_SPI_Pre_Transfer_Init(dev, timeout);
    if (bus_manager == NULL)
    {
        return TX_DELETED;
    }

    UINT status = TX_SUCCESS;

    // 接收第一次数据
    if (size1 > 0)
    {
        status = BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_RECEIVE, NULL, rx_data1, size1, timeout);
    }

    // 接收第二次数据（仅当第一次成功时）
    if (size2 > 0 && status == TX_SUCCESS)
    {
        status = BSP_SPI_Common_Transfer(dev, SPI_OP_TYPE_RECEIVE, NULL, rx_data2, size2, timeout);
    }

    BSP_SPI_Post_Transfer_Cleanup(bus_manager, dev);

    return status;
}

/*  SPI回调函数  */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(hspi);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, SPI_EVENT_TX_DONE_EVENT, TX_OR);
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(hspi);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, SPI_EVENT_RX_DONE_EVENT, TX_OR);
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(hspi);
    if (bus_manager != NULL)
    {
        tx_event_flags_set(&bus_manager->bus_event, SPI_EVENT_TX_RX_DONE_EVENT, TX_OR);
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    // 针对 OVR 错误的特殊处理 (STM32H7 适用)
    if (hspi->ErrorCode & HAL_SPI_ERROR_OVR)
    {
        volatile uint32_t tmpreg_sr = hspi->Instance->SR;
        volatile uint32_t tmpreg_dr = hspi->Instance->RXDR;
        (void)tmpreg_sr; // 防止编译器优化掉
        (void)tmpreg_dr; // 防止编译器优化掉
    }
    SPI_Bus_Manager *bus_manager = BSP_SPI_Get_Bus_Manager(hspi);
    if (bus_manager != NULL)
    {
        // 中止SPI传输
        HAL_SPI_Abort(hspi);
        hspi->State     = HAL_SPI_STATE_READY;
        hspi->ErrorCode = HAL_SPI_ERROR_NONE;
        // 设置错误事件标志，通知等待线程
        tx_event_flags_set(&bus_manager->bus_event, SPI_EVENT_ERROR_EVENT, TX_OR);
    }
}
