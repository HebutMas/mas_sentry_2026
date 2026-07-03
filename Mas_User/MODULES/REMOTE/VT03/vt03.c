/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-15 09:30:21
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-24 11:09:28
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/REMOTE/VT03/vt03.c
 * @Description:
 */
#include "vt03.h"
#include "bsp_def.h"
#include "crc_rm.h"
#include "module_offline.h"
#include "remote_data.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "remote_vt03"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#if defined(REMOTE_VT_SOURCE) && (REMOTE_VT_SOURCE == 2)

BUFFER_SECTION __attribute__((aligned(32))) static uint8_t VT03_buf[2][REMOTE_UART_RX_BUF_SIZE];
static VT03_Instance_t                                     vt03_instance;

UINT remote_vt03_init()
{
    // 重新初始化uart
    REMOTE_VT_UART.Init.BaudRate = 921600;
    if (HAL_UART_Init(&REMOTE_VT_UART) != HAL_OK)
    {
        log_e("uart init error");
        return TX_DELETED;
    }

    // 初始化sbus实例
    Offline_Init_config_t offline_init = {
        .name       = "vt03",
        .timeout_ms = 100,
        .beep_times = 0,
        .enable     = OFFLINE_ENABLE,
    };
    vt03_instance.offline_dev = Module_Offline_device_register(&offline_init);
    if (vt03_instance.offline_dev == NULL)
    {
        log_e("offline device register error");
        return TX_DELETED;
    }

    UART_Device_init_config sbus_cfg = {
        .huart           = &REMOTE_VT_UART,
        .expected_rx_len = 21,
        .rx_buf          = (uint8_t (*)[2])VT03_buf,
        .rx_buf_size     = REMOTE_UART_RX_BUF_SIZE,
        .rx_mode         = UART_MODE_DMA,
        .tx_mode         = UART_MODE_BLOCKING,
    };
    vt03_instance.uart_device = BSP_UART_Device_Init(&sbus_cfg);
    if (vt03_instance.uart_device == NULL)
    {
        log_e("uart device init error");
        return TX_DELETED;
    }

    vt03_instance.initialized = 1;

    log_i("vt03 init success");

    return TX_SUCCESS;
}

VT03_Instance_t *remote_get_vt03()
{
    if (vt03_instance.initialized)
    {
        return &vt03_instance;
    }
    return NULL;
}

void remote_vt03_decode_task_func()
{
    if (vt03_instance.initialized)
    {
        uint8_t *buf = BSP_UART_Read(vt03_instance.uart_device, TX_WAIT_FOREVER);
        if (buf == NULL && vt03_instance.uart_device->real_rx_len != 21)
        {
            return;
        }

        if (buf[0] == 0XA9 && buf[1] == 0X53)
        {
            if (Verify_CRC16_Check_Sum(buf, 21) == RM_TRUE)
            {
                // 原始数据
                vt03_instance.vt03_remote_data.ch1 = (buf[2] | (buf[3] << 8)) & 0x07FF;
                vt03_instance.vt03_remote_data.ch2 = ((buf[3] >> 3) | (buf[4] << 5)) & 0x07FF;
                vt03_instance.vt03_remote_data.ch3 =
                    ((buf[4] >> 6) | (buf[5] << 2) | (buf[6] << 10)) & 0x07FF;
                vt03_instance.vt03_remote_data.ch4 = ((buf[6] >> 1) | (buf[7] << 7)) & 0x07FF;

                // 数据边界检查
                if (vt03_instance.vt03_remote_data.ch1 < VT03_CH_VALUE_MIN ||
                    vt03_instance.vt03_remote_data.ch1 > VT03_CH_VALUE_MAX ||
                    vt03_instance.vt03_remote_data.ch2 < VT03_CH_VALUE_MIN ||
                    vt03_instance.vt03_remote_data.ch2 > VT03_CH_VALUE_MAX ||
                    vt03_instance.vt03_remote_data.ch3 < VT03_CH_VALUE_MIN ||
                    vt03_instance.vt03_remote_data.ch3 > VT03_CH_VALUE_MAX ||
                    vt03_instance.vt03_remote_data.ch4 < VT03_CH_VALUE_MIN ||
                    vt03_instance.vt03_remote_data.ch4 > VT03_CH_VALUE_MAX)
                {
                    return;
                }

                // 前四个通道减去VT03_CH_VALUE_OFFSET是为了让数据的中间值变为0,方便后续使用
                vt03_instance.vt03_remote_data.ch1 -= VT03_CH_VALUE_OFFSET;
                vt03_instance.vt03_remote_data.ch2 -= VT03_CH_VALUE_OFFSET;
                vt03_instance.vt03_remote_data.ch3 -= VT03_CH_VALUE_OFFSET;
                vt03_instance.vt03_remote_data.ch4 -= VT03_CH_VALUE_OFFSET;

                // 防止数据零漂，设置正负REMOTE_DEAD_ZONE的死区
                if (abs(vt03_instance.vt03_remote_data.ch1) <= REMOTE_DEAD_ZONE)
                    vt03_instance.vt03_remote_data.ch1 = 0;
                if (abs(vt03_instance.vt03_remote_data.ch2) <= REMOTE_DEAD_ZONE)
                    vt03_instance.vt03_remote_data.ch2 = 0;
                if (abs(vt03_instance.vt03_remote_data.ch3) <= REMOTE_DEAD_ZONE)
                    vt03_instance.vt03_remote_data.ch3 = 0;
                if (abs(vt03_instance.vt03_remote_data.ch4) <= REMOTE_DEAD_ZONE)
                    vt03_instance.vt03_remote_data.ch4 = 0;

                // 更新拨轮数据
                vt03_instance.vt03_remote_data.wheel = ((buf[8] >> 1) | (buf[9] << 7)) & 0x07FF;
                vt03_instance.vt03_remote_data.wheel -= VT03_CH_VALUE_OFFSET;

                // 更新按键状态
                vt03_instance.vt03_remote_data.switch_pos   = (buf[7] >> 4) & 0x03;
                vt03_instance.vt03_remote_data.pause        = (buf[7] >> 6) & 0x01;
                vt03_instance.vt03_remote_data.custom_left  = (buf[7] >> 7) & 0x01;
                vt03_instance.vt03_remote_data.custom_right = (buf[8] >> 0) & 0x01;
                vt03_instance.vt03_remote_data.trigger      = (buf[9] >> 4) & 0x01;

                // 更新鼠标数据
                vt03_instance.vt03_remote_data.mouse_state.mouse_x = buf[10] | (buf[11] << 8);
                vt03_instance.vt03_remote_data.mouse_state.mouse_y = buf[12] | (buf[13] << 8);
                vt03_instance.vt03_remote_data.mouse_state.mouse_z = buf[14] | (buf[15] << 8);
                vt03_instance.vt03_remote_data.mouse_state.mouse_l = buf[16] & 0x03;
                vt03_instance.vt03_remote_data.mouse_state.mouse_r = (buf[16] >> 2) & 0x03;
                vt03_instance.vt03_remote_data.mouse_state.mouse_m = (buf[16] >> 4) & 0x03;

                // 鼠标数据边界检查
                if (vt03_instance.vt03_remote_data.mouse_state.mouse_x < -32768 ||
                    vt03_instance.vt03_remote_data.mouse_state.mouse_x > 32767)
                {
                    vt03_instance.vt03_remote_data.mouse_state.mouse_x = 0;
                }
                if (vt03_instance.vt03_remote_data.mouse_state.mouse_y < -32768 ||
                    vt03_instance.vt03_remote_data.mouse_state.mouse_y > 32767)
                {
                    vt03_instance.vt03_remote_data.mouse_state.mouse_y = 0;
                }
                if (vt03_instance.vt03_remote_data.mouse_state.mouse_z < -32768 ||
                    vt03_instance.vt03_remote_data.mouse_state.mouse_z > 32767)
                {
                    vt03_instance.vt03_remote_data.mouse_state.mouse_z = 0;
                }

                // 更新键盘数据
                vt03_instance.vt03_remote_data.key_state.key_code = buf[17] | (buf[18] << 8);

                Module_Offline_device_update(vt03_instance.offline_dev);
            }
        }
    }
    else
    {
        tx_thread_sleep(100);
    }
}

int16_t remote_get_vt03_channel(uint8_t channel_index)
{
    if (!vt03_instance.initialized || channel_index < 1 || channel_index > 5)
    {
        return 0;
    }

    int16_t channel_value;
    switch (channel_index)
    {
    case 1:
        channel_value = vt03_instance.vt03_remote_data.ch1;
        break;
    case 2:
        channel_value = vt03_instance.vt03_remote_data.ch2;
        break;
    case 3:
        channel_value = vt03_instance.vt03_remote_data.ch3;
        break;
    case 4:
        channel_value = vt03_instance.vt03_remote_data.ch4;
        break;
    case 5:
        channel_value = vt03_instance.vt03_remote_data.wheel;
        break;
    default:
        channel_value = 0;
    }
    return channel_value;
}

mouse_state_t *remote_vt03_get_mouse()
{
    if (vt03_instance.initialized)
    {
        return &vt03_instance.vt03_remote_data.mouse_state;
    }
    return NULL;
}

keyboard_state_t *remote_vt03_get_keyboard()
{
    if (vt03_instance.initialized)
    {
        return &vt03_instance.vt03_remote_data.key_state;
    }
    return NULL;
}
#else
UINT remote_vt03_init()
{
    return TX_DELETED;
}

void remote_vt03_decode_task_func()
{
}

int16_t remote_get_vt03_channel(uint8_t channel_index)
{
    return 0;
}

mouse_state_t *remote_vt03_get_mouse()
{
    return NULL;
}

keyboard_state_t *remote_vt03_get_keyboard()
{
    return NULL;
}
#endif
