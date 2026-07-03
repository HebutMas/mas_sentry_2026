/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-15 09:30:03
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-24 10:09:52
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/REMOTE/VT02/vt02.c
 * @Description:
 */
#include "vt02.h"
#include "bsp_def.h"
#include "crc_rm.h"
#include "module_offline.h"
#include "remote_data.h"
#include <string.h>

#define LOG_TAG "remote_vt03"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#if defined(REMOTE_VT_SOURCE) && (REMOTE_VT_SOURCE == 1)

BUFFER_SECTION __attribute__((aligned(32))) static uint8_t vt02_buf[2][REMOTE_UART_RX_BUF_SIZE];
static VT02_Instance_t                                     vt02_instance;

UINT remote_vt02_init()
{
    // 重新初始化uart
    REMOTE_VT_UART.Init.BaudRate = 115200;
    if (HAL_UART_Init(&REMOTE_VT_UART) != HAL_OK)
    {
        log_e("uart init error");
        return TX_DELETED;
    }

    // 初始化sbus实例
    Offline_Init_config_t offline_init = {
        .name       = "vt02",
        .timeout_ms = 100,
        .beep_times = 0,
        .enable     = OFFLINE_ENABLE,
    };
    vt02_instance.offline_dev = Module_Offline_device_register(&offline_init);
    if (vt02_instance.offline_dev == NULL)
    {
        log_e("offline device register error");
        return TX_DELETED;
    }

    UART_Device_init_config sbus_cfg = {
        .huart           = &REMOTE_VT_UART,
        .expected_rx_len = 0,
        .rx_buf          = (uint8_t (*)[2])vt02_buf,
        .rx_buf_size     = REMOTE_UART_RX_BUF_SIZE,
        .rx_mode         = UART_MODE_DMA,
        .tx_mode         = UART_MODE_BLOCKING,
    };
    vt02_instance.uart_device = BSP_UART_Device_Init(&sbus_cfg);
    if (vt02_instance.uart_device == NULL)
    {
        log_e("uart device init error");
        return TX_DELETED;
    }

    vt02_instance.initialized = 1;

    log_i("vt02 init success");

    return TX_SUCCESS;
}

VT02_Instance_t *remote_get_vt02()
{
    if (vt02_instance.initialized)
    {
        return &vt02_instance;
    }
    return NULL;
}

void remote_vt02_decode_task_func()
{
    if (vt02_instance.initialized)
    {
        uint8_t *buf = BSP_UART_Read(vt02_instance.uart_device, TX_WAIT_FOREVER);
        if (buf == NULL)
        {
            return;
        }

        // 验证帧头CRC8校验
        if (!Verify_CRC8_Check_Sum(buf, 5))
        {
            return;
        }

        // 判断是否为键鼠遥控数据(0x0304)
        uint16_t cmd_id = (buf[6] << 8) | buf[5];
        if (cmd_id != 0x0304)
        {
            return;
        }

        // 解析键鼠遥控数据 (从偏移量7开始)
        uint8_t *data_ptr = &buf[7];

        // 鼠标
        vt02_instance.vt02_remote_data.mouse_state.mouse_x =
            (int16_t)((data_ptr[1] << 8) | data_ptr[0]);
        vt02_instance.vt02_remote_data.mouse_state.mouse_y =
            (int16_t)((data_ptr[3] << 8) | data_ptr[2]);
        vt02_instance.vt02_remote_data.mouse_state.mouse_z =
            (int16_t)((data_ptr[5] << 8) | data_ptr[4]);
        vt02_instance.vt02_remote_data.mouse_state.mouse_l = data_ptr[6];
        vt02_instance.vt02_remote_data.mouse_state.mouse_r = data_ptr[7];

        // 键盘
        vt02_instance.vt02_remote_data.key_state.key_code = (data_ptr[9] << 8) | data_ptr[8];

        Module_Offline_device_update(vt02_instance.offline_dev);
    }
    else
    {
        tx_thread_sleep(100);
    }
}

mouse_state_t *remote_vt02_get_mouse()
{
    if (vt02_instance.initialized)
    {
        return &vt02_instance.vt02_remote_data.mouse_state;
    }
    return NULL;
}

keyboard_state_t *remote_vt02_get_keyboard()
{
    if (vt02_instance.initialized)
    {
        return &vt02_instance.vt02_remote_data.key_state;
    }
    return NULL;
}
#else
UINT remote_vt02_init()
{
    return TX_DELETED;
}
void remote_vt02_decode_task_func()
{
}
mouse_state_t *remote_vt02_get_mouse()
{
    return NULL;
}
keyboard_state_t *remote_vt02_get_keyboard()
{
    return NULL;
}
#endif
