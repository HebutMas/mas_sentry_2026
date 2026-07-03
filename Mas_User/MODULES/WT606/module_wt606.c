#include "module_wt606.h"
#include "arm_math.h"
#include "bsp_def.h"

#define LOG_TAG "module_wt"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static Module_WT606_Device_t wt606_device;

BUFFER_SECTION __attribute__((aligned(32))) static uint8_t data_buffer[2][128];

void DWT606_Parse(uint8_t *buf, uint16_t len)
{
    // 至少需要 11 个字节才能组成一个有效帧
    if (len < 11)
        return;

    for (uint16_t i = 0; i < len - 10; i++)
    {
        if (buf[i] == 0X55)
        {
            uint8_t type = buf[i+1];
            uint8_t sum  = 0;

            // 校验和计算：累加前10字节
            for (uint8_t j = 0; j < 10; j++)
            {
                sum += buf[i+j];
            }

            // 校验和必须匹配 (buf[i+10] 是第11字节)
            if (sum == buf[i+10])
            {
                // 指向数据域起始位置 (跳过 Header 和 Type)
                uint8_t *data_ptr = &buf[i+2];

                if (type == 0x51) // 加速度 + 温度
                {
                    wt606_device.acc[0] =
                        (int16_t)((int16_t)data_ptr[1] << 8 | data_ptr[0]) / 32768.0f * 16.0f;
                    wt606_device.acc[1] =
                        (int16_t)((int16_t)data_ptr[3] << 8 | data_ptr[2]) / 32768.0f * 16.0f;
                    wt606_device.acc[2] =
                        (int16_t)((int16_t)data_ptr[5] << 8 | data_ptr[4]) / 32768.0f * 16.0f;
                    // Temp (忽略)
                }
                else if (type == 0x52) // 角速度 + 电压
                {
                    wt606_device.gyro_dps[0] =
                        (int16_t)((int16_t)data_ptr[1] << 8 | data_ptr[0]) / 32768.0f * 2000.0f;
                    wt606_device.gyro_dps[1] =
                        (int16_t)((int16_t)data_ptr[3] << 8 | data_ptr[2]) / 32768.0f * 2000.0f;
                    wt606_device.gyro_dps[2] =
                        (int16_t)((int16_t)data_ptr[5] << 8 | data_ptr[4]) / 32768.0f * 2000.0f;

                    wt606_device.gyro_rps[0] = wt606_device.gyro_dps[0] / 57.3f;
                    wt606_device.gyro_rps[1] = wt606_device.gyro_dps[1] / 57.3f;
                    wt606_device.gyro_rps[2] = wt606_device.gyro_dps[2] / 57.3f;
                }
                else if (type == 0x53) // 角度 + 版本
                {

                    wt606_device.Euler_degree[0] =
                        (int16_t)((int16_t)data_ptr[1] << 8 | data_ptr[0]) / 32768.0f * 180.0f;
                    wt606_device.Euler_degree[1] =
                        (int16_t)((int16_t)data_ptr[3] << 8 | data_ptr[2]) / 32768.0f * 180.0f;
                    wt606_device.Euler_degree[2] =
                        (int16_t)((int16_t)data_ptr[5] << 8 | data_ptr[4]) / 32768.0f * 180.0f;
                    if (wt606_device.Euler_degree[2] - wt606_device.last_yaw_degree > 180.0f)
                    {
                        wt606_device.YawRoundCount--;
                    }
                    else if (wt606_device.Euler_degree[2] - wt606_device.last_yaw_degree < -180.0f)
                    {
                        wt606_device.YawRoundCount++;
                    }
                    wt606_device.last_yaw_degree = wt606_device.Euler_degree[2];
                    wt606_device.YawTotalAngle_degree =
                        wt606_device.Euler_degree[2] + 360.0f * wt606_device.YawRoundCount;

                    wt606_device.Euler_rad[0] = wt606_device.Euler_degree[0] * PI / 180.0f;
                    wt606_device.Euler_rad[1] = wt606_device.Euler_degree[1] * PI / 180.0f;
                    wt606_device.Euler_rad[2] = wt606_device.Euler_degree[2] * PI / 180.0f;
                    wt606_device.YawTotalAngle_rad =
                        wt606_device.YawTotalAngle_degree * PI / 180.0f;
                }
                Module_Offline_device_update(wt606_device.offline_dev);
            }
        }
    }
}

UINT Module_WT606_init()
{
    // 重新初始化uart
    WT606_UART.Init.BaudRate = 921600;
    if (HAL_UART_Init(&WT606_UART) != HAL_OK)
    {
        log_e("uart init error");
        return TX_DELETED;
    }
    UART_Device_init_config config = {.huart           = &WT606_UART,
                                      .expected_rx_len = 0,
                                      .rx_buf          = (uint8_t (*)[2])data_buffer,
                                      .rx_buf_size     = 128,
                                      .tx_mode         = UART_MODE_DMA,
                                      .rx_mode         = UART_MODE_DMA};
    wt606_device.uart_dev          = BSP_UART_Device_Init(&config);
    if (wt606_device.uart_dev == NULL)
    {
        log_e("uart device init error");
        return TX_DELETED;
    }
    Offline_Init_config_t offlineconfig = {.name       = "wt606",
                                           .beep_times = WT606_OFFLINE_BEEP_TIMES,
                                           .enable     = OFFLINE_ENABLE,
                                           .timeout_ms = 100};
    wt606_device.offline_dev            = Module_Offline_device_register(&offlineconfig);
    if (wt606_device.offline_dev == NULL)
    {
        log_e("offline device register error");
        return TX_DELETED;
    }

    wt606_device.initialized = 1;

    return TX_SUCCESS;
}

void Module_WT606_task_function(void)
{
    if (wt606_device.initialized == 1)
    {
        uint8_t *buf = BSP_UART_Read(wt606_device.uart_dev, TX_WAIT_FOREVER);
        uint16_t len = wt606_device.uart_dev->real_rx_len;
        if (buf != NULL && len > 0)
        {
            DWT606_Parse(buf, len);
        }
    }
    else
    {
        tx_thread_sleep(10);
    }
}

Module_WT606_Device_t *Module_WT606_get_device()
{
    if (wt606_device.initialized)
    {
        return &wt606_device;
    }
    return NULL;
}
