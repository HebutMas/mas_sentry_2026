/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-15 09:29:38
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-24 10:59:34
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/REMOTE/SBUS/sbus.c
 * @Description:
 */
#include "sbus.h"
#include "bsp_def.h"
#include "bsp_uart.h"
#include "module_offline.h"
#include "remote_data.h"
#include <stdint.h>

#define LOG_TAG "remote_sbus"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#if defined(REMOTE_SOURCE) && (REMOTE_SOURCE == 1)

BUFFER_SECTION __attribute__((aligned(32))) static uint8_t sbus_buf[REMOTE_UART_RX_BUF_SIZE][2];
static SBUS_Instance_t                                     sbus_instance;

UINT remote_sbus_init()
{
    // 重新初始化uart
    REMOTE_UART.Init.BaudRate = 100000;
    if (HAL_UART_Init(&REMOTE_UART) != HAL_OK)
    {
        log_e("uart init error");
        return TX_DELETED;
    }

    // 初始化sbus实例
    Offline_Init_config_t offline_init = {
        .name       = "sbus",
        .timeout_ms = 50,
        .beep_times = 0,
        .enable     = OFFLINE_ENABLE,
    };
    sbus_instance.offline_dev = Module_Offline_device_register(&offline_init);
    if (sbus_instance.offline_dev == NULL)
    {
        log_e("offline device register error");
        return TX_DELETED;
    }

    UART_Device_init_config sbus_cfg = {
        .huart           = &REMOTE_UART,
        .expected_rx_len = 25,
        .rx_buf          = (uint8_t (*)[2])sbus_buf,
        .rx_buf_size     = REMOTE_UART_RX_BUF_SIZE,
        .rx_mode         = UART_MODE_DMA,
        .tx_mode         = UART_MODE_BLOCKING,
    };
    sbus_instance.uart_device = BSP_UART_Device_Init(&sbus_cfg);
    if (sbus_instance.uart_device == NULL)
    {
        log_e("uart device init error");
        return TX_DELETED;
    }

    sbus_instance.initialized = 1;

    log_i("sbus init success");

    return TX_SUCCESS;
}

SBUS_Instance_t *remote_get_sbus()
{
    if (sbus_instance.initialized)
    {
        return &sbus_instance;
    }
    return NULL;
}

void remote_sbus_decode_task_func()
{
    if (sbus_instance.initialized)
    {
        uint8_t *buf = BSP_UART_Read(sbus_instance.uart_device, TX_WAIT_FOREVER);

        // 验证帧头和帧尾和长度
        if (buf != NULL && buf[0] == 0x0F && buf[24] == 0x00 && sbus_instance.uart_device->real_rx_len == 25)
        {
            // 获取原始数据
            sbus_instance.SBUS_CH.CH1 = ((int16_t)buf[1] >> 0 | ((int16_t)buf[2] << 8)) & 0x07FF;
            sbus_instance.SBUS_CH.CH2 = ((int16_t)buf[2] >> 3 | ((int16_t)buf[3] << 5)) & 0x07FF;
            sbus_instance.SBUS_CH.CH3 =
                ((int16_t)buf[3] >> 6 | ((int16_t)buf[4] << 2) | (int16_t)buf[5] << 10) & 0x07FF;
            sbus_instance.SBUS_CH.CH4 = ((int16_t)buf[5] >> 1 | ((int16_t)buf[6] << 7)) & 0x07FF;

            // 通道数据边界检查
            if (sbus_instance.SBUS_CH.CH1 < SBUS_CHX_UP ||
                sbus_instance.SBUS_CH.CH1 > SBUS_CHX_DOWN)
                sbus_instance.SBUS_CH.CH1 = SBUS_CHX_BIAS;
            if (sbus_instance.SBUS_CH.CH2 < SBUS_CHX_UP ||
                sbus_instance.SBUS_CH.CH2 > SBUS_CHX_DOWN)
                sbus_instance.SBUS_CH.CH2 = SBUS_CHX_BIAS;
            if (sbus_instance.SBUS_CH.CH3 < SBUS_CHX_UP ||
                sbus_instance.SBUS_CH.CH3 > SBUS_CHX_DOWN)
                sbus_instance.SBUS_CH.CH3 = SBUS_CHX_BIAS;
            if (sbus_instance.SBUS_CH.CH4 < SBUS_CHX_UP ||
                sbus_instance.SBUS_CH.CH4 > SBUS_CHX_DOWN)
                sbus_instance.SBUS_CH.CH4 = SBUS_CHX_BIAS;

            // 前四个通道减去SBUS_CHX_BIAS是为了让数据的中间值变为0,方便后续使用
            sbus_instance.SBUS_CH.CH1 -= SBUS_CHX_BIAS;
            sbus_instance.SBUS_CH.CH2 -= SBUS_CHX_BIAS;
            sbus_instance.SBUS_CH.CH3 -= SBUS_CHX_BIAS;
            sbus_instance.SBUS_CH.CH4 -= SBUS_CHX_BIAS;

            // 防止数据零漂，设置死区
            if (abs(sbus_instance.SBUS_CH.CH1) <= REMOTE_DEAD_ZONE)
                sbus_instance.SBUS_CH.CH1 = 0;
            if (abs(sbus_instance.SBUS_CH.CH2) <= REMOTE_DEAD_ZONE)
                sbus_instance.SBUS_CH.CH2 = 0;
            if (abs(sbus_instance.SBUS_CH.CH3) <= REMOTE_DEAD_ZONE)
                sbus_instance.SBUS_CH.CH3 = 0;
            if (abs(sbus_instance.SBUS_CH.CH4) <= REMOTE_DEAD_ZONE)
                sbus_instance.SBUS_CH.CH4 = 0;

            // 其余通道读取
            sbus_instance.SBUS_CH.CH5 = ((int16_t)buf[6] >> 4 | ((int16_t)buf[7] << 4)) & 0x07FF;
            sbus_instance.SBUS_CH.CH6 =
                ((int16_t)buf[7] >> 7 | ((int16_t)buf[8] << 1) | (int16_t)buf[9] << 9) & 0x07FF;
            sbus_instance.SBUS_CH.CH7  = ((int16_t)buf[9] >> 2 | ((int16_t)buf[10] << 6)) & 0x07FF;
            sbus_instance.SBUS_CH.CH8  = ((int16_t)buf[10] >> 5 | ((int16_t)buf[11] << 3)) & 0x07FF;
            sbus_instance.SBUS_CH.CH9  = ((int16_t)buf[12] << 0 | ((int16_t)buf[13] << 8)) & 0x07FF;
            sbus_instance.SBUS_CH.CH10 = ((int16_t)buf[13] >> 3 | ((int16_t)buf[14] << 5)) & 0x07FF;
            sbus_instance.SBUS_CH.CH11 =
                ((int16_t)buf[14] >> 6 | ((int16_t)buf[15] << 2) | (int16_t)buf[16] << 10) & 0x07FF;
            sbus_instance.SBUS_CH.CH12 = ((int16_t)buf[16] >> 1 | ((int16_t)buf[17] << 7)) & 0x07FF;
            sbus_instance.SBUS_CH.CH13 = ((int16_t)buf[17] >> 4 | ((int16_t)buf[18] << 4)) & 0x07FF;
            sbus_instance.SBUS_CH.CH14 =
                ((int16_t)buf[18] >> 7 | ((int16_t)buf[19] << 1) | (int16_t)buf[20] << 9) & 0x07FF;
            sbus_instance.SBUS_CH.CH15 = ((int16_t)buf[20] >> 2 | ((int16_t)buf[21] << 6)) & 0x07FF;
            sbus_instance.SBUS_CH.CH16 = ((int16_t)buf[21] >> 5 | ((int16_t)buf[22] << 3)) & 0x07FF;
            sbus_instance.SBUS_CH.ConnectState = buf[23];

            if (sbus_instance.SBUS_CH.ConnectState == 0x00)
            {
                Module_Offline_device_update(sbus_instance.offline_dev);
            }
        }
    }
}

int16_t remote_get_sbus_channel(uint8_t channel_index)
{
    if (!sbus_instance.initialized || channel_index < 1 || channel_index > 16)
    {
        return 0;
    }

    uint16_t channel_value;

    switch (channel_index)
    {
    case 1:
        channel_value = sbus_instance.SBUS_CH.CH1;
        break;
    case 2:
        channel_value = sbus_instance.SBUS_CH.CH2;
        break;
    case 3:
        channel_value = sbus_instance.SBUS_CH.CH3;
        break;
    case 4:
        channel_value = sbus_instance.SBUS_CH.CH4;
        break;
    case 5:
        channel_value = sbus_instance.SBUS_CH.CH5;
        break;
    case 6:
        channel_value = sbus_instance.SBUS_CH.CH6;
        break;
    case 7:
        channel_value = sbus_instance.SBUS_CH.CH7;
        break;
    case 8:
        channel_value = sbus_instance.SBUS_CH.CH8;
        break;
    case 9:
        channel_value = sbus_instance.SBUS_CH.CH9;
        break;
    case 10:
        channel_value = sbus_instance.SBUS_CH.CH10;
        break;
    case 11:
        channel_value = sbus_instance.SBUS_CH.CH11;
        break;
    case 12:
        channel_value = sbus_instance.SBUS_CH.CH12;
        break;
    case 13:
        channel_value = sbus_instance.SBUS_CH.CH13;
        break;
    case 14:
        channel_value = sbus_instance.SBUS_CH.CH14;
        break;
    case 15:
        channel_value = sbus_instance.SBUS_CH.CH15;
        break;
    case 16:
        channel_value = sbus_instance.SBUS_CH.CH16;
        break;
    default:
        channel_value = 0;
        break;
    }
    return channel_value;
}
#else
UINT remote_sbus_init()
{
    return TX_DELETED;
}
void remote_sbus_decode_task_func()
{
}
int16_t remote_get_sbus_channel(uint8_t channel_index)
{
    return 0;
}
#endif
