/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-14 16:49:17
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-27 19:28:44
 * @FilePath: /MAS_RM_DJIC/Mas_User/BSP/PWM/bsp_pwm.c
 * @Description:
 */

#include "bsp_def.h"
#include "bsp_pwm.h"
#include <string.h>

#if defined(STM32H723xx)
#include "core_cm7.h"
#elif defined(STM32F407xx)
#endif

#define LOG_TAG "bsp_pwm"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

/* PWM设备链表头指针 */
static PWM_Device *pwm_device_list = NULL;

// 内部辅助函数
static PWM_Device *PWM_Find_Device(TIM_HandleTypeDef *htim, uint32_t channel)
{
    PWM_Device *dev = pwm_device_list;

    while (dev != NULL)
    {
        if (dev->htim == htim && dev->Channel == channel)
        {
            return dev;
        }
        dev = dev->next;
    }

    return NULL;
}

static uint32_t PWM_Convert_Dutyx10_To_Pulse(PWM_Device *dev)
{
    /* dutyx10范围：0-1000，对应占空比0%-100%，比较值 = Period * dutyx10 / 1000 */
    return (dev->Period * dev->dutyx10) / 1000;
}

// 对外接口
PWM_Device *BSP_PWM_Device_Init(PWM_Init_Config *config)
{
    PWM_Device *dev = NULL;
    UINT        status;

    /* 参数检查 */
    if (config == NULL || config->htim == NULL)
    {
        log_e("Invalid parameters: config=%p, htim=%p", config, config ? config->htim : NULL);
        return NULL;
    }

    /* 检查是否已存在相同设备 */
    if (PWM_Find_Device(config->htim, config->channel) != NULL)
    {
        log_e("PWM device already exists: TIM%lu channel %lu",
              (unsigned long)((uintptr_t)config->htim), config->channel);
        return NULL;
    }

    /* 分配PWM设备内存 */
    BSP_MEM_ALLOC_WAIT(dev, sizeof(PWM_Device), TX_NO_WAIT);
    if (dev == NULL)
    {
        return NULL;
    }

    /* 清零内存 */
    memset(dev, 0, sizeof(PWM_Device));

    /* 配置PWM基本参数（从CubeMX配置中读取） */
    dev->htim    = config->htim;
    dev->Channel = config->channel;
    dev->Period  = config->htim->Init.Period; // 从CubeMX配置读取周期
    dev->dutyx10 = config->dutyx10;
    dev->Mode    = config->Mode;

    /* 初始化ThreadX事件标志组 */
    status = tx_event_flags_create(&dev->pwm_event, NULL);
    if (status != TX_SUCCESS)
    {
        BSP_MEM_FREE(dev);
        return NULL;
    }

    /* 将设备加入链表 */
    dev->next       = pwm_device_list;
    pwm_device_list = dev;

    log_i("PWM device initialized successfully");

    return dev;
}

UINT BSP_PWM_Device_DeInit(PWM_Device *dev)
{
    PWM_Device **current = &pwm_device_list;

    if (dev == NULL)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    /* 停止PWM */
    BSP_PWM_Stop(dev);

    /* 从链表中移除设备 */
    while (*current != NULL)
    {
        if (*current == dev)
        {
            *current = dev->next;
            break;
        }
        current = &(*current)->next;
    }

    log_i("PWM device deinitialized: TIM%lu channel %lu", (unsigned long)((uintptr_t)dev->htim),
          dev->Channel);

    /* 删除事件标志组 */
    tx_event_flags_delete(&dev->pwm_event);

    /* 释放内存 */
    BSP_MEM_FREE(dev);

    return TX_SUCCESS;
}

UINT BSP_PWM_Start(PWM_Device *dev, uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef hal_status;
    uint32_t          hal_channel;
    uint32_t         *dma_buffer;
    uint16_t          dma_length;

    if (dev == NULL || dev->htim == NULL)
    {
        log_e("Invalid device parameters");
        return TX_DELETED;
    }

    hal_channel = dev->Channel;

    switch (dev->Mode)
    {
    case PWM_MODE_BLOCKING:
        hal_status = HAL_TIM_PWM_Start(dev->htim, hal_channel);
        break;

    case PWM_MODE_IT:
        hal_status = HAL_TIM_PWM_Start_IT(dev->htim, hal_channel);
        break;

    case PWM_MODE_DMA:
        if (data != NULL && len > 0)
        {
            dma_buffer = (uint32_t *)data;
            dma_length = len;
        }
        else
        {
            /* 如果没有提供数据，返回错误 */
            log_e("DMA mode requires valid data buffer and length");
            return TX_DELETED;
        }
#if defined(STM32H723xx)
        /* 清除数据缓存，确保DMA读取最新数据 */
        uint32_t start = (uint32_t)dma_buffer & ~0x1F;
        uint32_t end   = ((uint32_t)dma_buffer + dma_length * sizeof(uint32_t) + 31) & ~0x1F;
        SCB_CleanDCache_by_Addr((uint32_t *)start, end - start);
#endif

        hal_status = HAL_TIM_PWM_Start_DMA(dev->htim, hal_channel, dma_buffer, dma_length);
        break;

    default:
        log_e("Invalid PWM mode: %d", dev->Mode);
        return TX_DELETED;
    }

    if (hal_status != HAL_OK)
    {
        log_e("Failed to start PWM: HAL status=%d", hal_status);
        return TX_DELETED;
    }
    return TX_SUCCESS;
}

UINT BSP_PWM_Stop(PWM_Device *dev)
{
    HAL_StatusTypeDef hal_status;
    uint32_t          hal_channel;

    if (dev == NULL || dev->htim == NULL)
    {
        log_e("Invalid device parameters");
        return TX_DELETED;
    }

    hal_channel = dev->Channel;

    /* 根据不同模式停止PWM */
    switch (dev->Mode)
    {
    case PWM_MODE_BLOCKING:
        hal_status = HAL_TIM_PWM_Stop(dev->htim, hal_channel);
        break;
    case PWM_MODE_IT:
        hal_status = HAL_TIM_PWM_Stop_IT(dev->htim, hal_channel);
        break;
    case PWM_MODE_DMA:
        hal_status = HAL_TIM_PWM_Stop_DMA(dev->htim, hal_channel);
        break;

    default:
        log_e("Invalid PWM mode: %d", dev->Mode);
        return TX_DELETED;
    }

    if (hal_status != HAL_OK)
    {
        log_e("Failed to stop PWM: HAL status=%d", hal_status);
        return TX_DELETED;
    }
    return TX_SUCCESS;
}

UINT BSP_PWM_SetDutyCycle(PWM_Device *dev, uint16_t duty)
{
    uint32_t hal_channel;
    uint32_t pulse;

    if (dev == NULL)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    /* 限制duty范围0-1000 */
    if (duty > 1000)
    {
        log_w("Duty cycle out of range, clamped to 1000");
        duty = 1000;
    }

    dev->dutyx10 = duty;
    pulse        = PWM_Convert_Dutyx10_To_Pulse(dev);
    hal_channel  = dev->Channel;

    /* 设置比较值 */
    __HAL_TIM_SET_COMPARE(dev->htim, hal_channel, pulse);

    return TX_SUCCESS;
}

UINT BSP_PWM_SetPulse(PWM_Device *dev, uint32_t pulse)
{
    uint32_t hal_channel;

    if (dev == NULL)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    /* 限制pulse范围不超过Period */
    if (pulse > dev->Period)
    {
        log_w("Pulse out of range, clamped to Period=%lu", dev->Period);
        pulse = dev->Period;
    }

    /* 更新dutyx10 */
    dev->dutyx10 = (pulse * 1000) / dev->Period;

    hal_channel = dev->Channel;

    /* 设置比较值 */
    __HAL_TIM_SET_COMPARE(dev->htim, hal_channel, pulse);

    return TX_SUCCESS;
}

UINT BSP_PWM_SetPSC(PWM_Device *dev, uint32_t psc)
{
    if (dev == NULL || dev->htim == NULL)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    __HAL_TIM_SET_PRESCALER(dev->htim, psc);

    return TX_SUCCESS;
}

UINT BSP_PWM_WaitForComplete(PWM_Device *dev, ULONG wait_option)
{
    ULONG actual_flags;
    UINT  status;

    if (dev == NULL)
    {
        log_e("Invalid device pointer");
        return TX_DELETED;
    }

    /* 等待完成或错误事件 */
    status = tx_event_flags_get(&dev->pwm_event, PWM_EVENT_DONE | PWM_EVENT_ERROR, TX_OR_CLEAR,
                                &actual_flags, wait_option);

    if (status != TX_SUCCESS)
    {
        log_e("Wait timeout or error: status=%d", status);
        return status;
    }

    /* 检查是否有错误发生 */
    if (actual_flags & PWM_EVENT_ERROR)
    {
        log_e("PWM operation failed: ERROR flag set");
        return TX_DELETED;
    }

    return TX_SUCCESS;
}

// 中断回调函数
__attribute__((section(".itcmram"))) void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    PWM_Device *dev = NULL;

    /* 查找对应的PWM设备 */
    dev = pwm_device_list;
    while (dev != NULL)
    {
        if (dev->htim == htim)
        {
            /* 设置完成事件标志 */
            tx_event_flags_set(&dev->pwm_event, PWM_EVENT_DONE, TX_OR);
            log_d("PWM pulse finished callback: TIM%lu channel %lu",
                  (unsigned long)((uintptr_t)dev->htim), dev->Channel);
            break;
        }
        dev = dev->next;
    }
}

__attribute__((section(".itcmram"))) void HAL_TIM_ErrorCallback(TIM_HandleTypeDef *htim)
{
    PWM_Device *dev = NULL;

    /* 查找对应的PWM设备 */
    dev = pwm_device_list;
    while (dev != NULL)
    {
        if (dev->htim == htim)
        {
            /* 设置错误事件标志 */
            tx_event_flags_set(&dev->pwm_event, PWM_EVENT_ERROR, TX_OR);
            log_e("PWM error callback: TIM%lu channel %lu", (unsigned long)((uintptr_t)dev->htim),
                  dev->Channel);
            break;
        }
        dev = dev->next;
    }
}
