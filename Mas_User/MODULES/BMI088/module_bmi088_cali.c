/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-11 15:04:31
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-22 18:50:58
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/BMI088/module_bmi088_cali.c
 * @Description:
 */
#include "module_bmi088.h"
#include "bsp_dwt.h"
#include "math.h"
#include "module_rgb.h"

#if defined(STM32H723xx)
#include "module_w25q64.h"
#elif defined(STM32F407xx)
#include "bsp_flash.h"
#endif

#define LOG_TAG "module_bmi088"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define GxOFFSET -0.00601393497f
#define GyOFFSET -0.00196841615f
#define GzOFFSET 0.00114696583f
#define gNORM    9.67463112f

UINT Module_BMI088_calibrate_bmi088_offset(Bmi088_device_t *dev)
{
    float                       gyroDiff[3];
    static float                gNormDiff;
    static BMI088_Cali_Offset_t caliOffset;

    // 检查空指针
    if (dev == NULL || !dev->initialized)
    {
        log_e("BMI088 instance is NULL");
        return TX_DELETED;
    }

    UINT     status = TX_SUCCESS;
    float    dt, t;
    uint32_t dt_cnt;
    uint16_t CaliTimes = 6000;
    float    gyroMax[3], gyroMin[3];
    float    gNormTemp = 0.0f, gNormMax = 0.0f, gNormMin = 0.0f;

    Module_Rgb_show(LED_Yellow);

    // 初始化变量
    dt     = 0;
    t      = 0;
    dt_cnt = 0;

    // 初始化校准偏移量
    memset(&caliOffset, 0, sizeof(BMI088_Cali_Offset_t));

    do
    {
        dt = DWT_GetDeltaT(&dt_cnt);
        t += dt;

        // 超时保护，避免无限循环
        if (t > (CaliTimes / 1000.0f) + 1.0f)
        {
            caliOffset.GyroOffset[0] = GxOFFSET;
            caliOffset.GyroOffset[1] = GyOFFSET;
            caliOffset.GyroOffset[2] = GzOFFSET;
            caliOffset.gNorm         = gNORM;
            caliOffset.TempWhenCali  = 40;
            caliOffset.Calibrated    = 1;
            caliOffset.AccelScale    = 9.81f / gNORM;
            log_e("Calibrate BMI088 Offset Timeout!");
            Module_Rgb_show(LED_Red);
            status = TX_DELETED;
            break;
        }

        DWT_Delay(0.005f);
        caliOffset.gNorm         = 0;
        caliOffset.GyroOffset[0] = 0;
        caliOffset.GyroOffset[1] = 0;
        caliOffset.GyroOffset[2] = 0;
        caliOffset.Calibrated    = 0;

        // 初始化最大最小值
        gNormMax = 0.0f;
        gNormMin = 100.0f; // 一个较大的初始值
        for (uint8_t j = 0; j < 3; j++)
        {
            gyroMax[j] = -100.0f; // 一个较小的初始值
            gyroMin[j] = 100.0f;  // 一个较大的初始值
        }

        uint16_t valid_samples = 0;
        for (uint16_t i = 0; i < CaliTimes && valid_samples < CaliTimes; i++)
        {
            // 获取加速度数据
            if (Module_BMI088_get_accel() != TX_SUCCESS)
            {
                log_w("Failed to get accel data at iteration %d", i);
                DWT_Delay(0.001f);
                continue;
            }

            gNormTemp = sqrtf(dev->acc[0] * dev->acc[0] + dev->acc[1] * dev->acc[1] +
                              dev->acc[2] * dev->acc[2]);

            // 获取陀螺仪数据
            if (Module_BMI088_get_gyro() != TX_SUCCESS)
            {
                log_w("Failed to get gyro data at iteration %d", i);
                DWT_Delay(0.001f);
                continue;
            }

            // 数据验证，排除异常值
            if (isnan(gNormTemp) || isinf(gNormTemp) || isnan(dev->gyro[0]) ||
                isnan(dev->gyro[1]) || isnan(dev->gyro[2]) || isinf(dev->gyro[0]) ||
                isinf(dev->gyro[1]) || isinf(dev->gyro[2]))
            {
                DWT_Delay(0.001f);
                continue;
            }

            // 检查数据范围是否合理
            if (gNormTemp > 15.0f || gNormTemp < 5.0f || // 重力加速度应在9.8左右
                fabsf(dev->gyro[0]) > 5.0f || fabsf(dev->gyro[1]) > 5.0f ||
                fabsf(dev->gyro[2]) > 5.0f)
            { // 角速度应在较小范围内
                DWT_Delay(0.001f);
                continue;
            }

            caliOffset.gNorm += gNormTemp;
            caliOffset.GyroOffset[0] += dev->gyro[0];
            caliOffset.GyroOffset[1] += dev->gyro[1];
            caliOffset.GyroOffset[2] += dev->gyro[2];
            valid_samples++;

            if (valid_samples == 1)
            {
                gNormMax = gNormTemp;
                gNormMin = gNormTemp;
                for (uint8_t j = 0; j < 3; j++)
                {
                    gyroMax[j] = dev->gyro[j];
                    gyroMin[j] = dev->gyro[j];
                }
            }
            else
            {
                if (gNormTemp > gNormMax)
                    gNormMax = gNormTemp;
                if (gNormTemp < gNormMin)
                    gNormMin = gNormTemp;
                for (uint8_t j = 0; j < 3; j++)
                {
                    if (dev->gyro[j] > gyroMax[j])
                        gyroMax[j] = dev->gyro[j];
                    if (dev->gyro[j] < gyroMin[j])
                        gyroMin[j] = dev->gyro[j];
                }
            }
            gNormDiff = gNormMax - gNormMin;
            for (uint8_t j = 0; j < 3; j++)
            {
                gyroDiff[j] = gyroMax[j] - gyroMin[j];
            }

            // 检查是否稳定，如果设备移动则重新开始采样
            if (gNormDiff > 0.5f || gyroDiff[0] > 0.15f || gyroDiff[1] > 0.15f ||
                gyroDiff[2] > 0.15f)
            {
                // 设备移动了，重新开始采样
                log_w("Device moved during calibration, restarting...");
                valid_samples            = 0;
                caliOffset.gNorm         = 0;
                caliOffset.GyroOffset[0] = 0;
                caliOffset.GyroOffset[1] = 0;
                caliOffset.GyroOffset[2] = 0;
                gNormMax                 = 0.0f;
                gNormMin                 = 100.0f;
                for (uint8_t j = 0; j < 3; j++)
                {
                    gyroMax[j] = -100.0f;
                    gyroMin[j] = 100.0f;
                }
                // 等待设备稳定
                DWT_Delay(0.5f);
            }

            DWT_Delay(0.001f);
        }

        // 使用有效采样数计算平均值
        if (valid_samples > 0)
        {
            caliOffset.gNorm /= (float)valid_samples;
            for (uint8_t i = 0; i < 3; i++)
            {
                caliOffset.GyroOffset[i] /= (float)valid_samples;
            }
        }
        else
        {
            log_e("No valid samples collected during calibration");
            status = TX_DELETED;
            break;
        }

        // 记录标定时的温度
        if (Module_BMI088_get_temp() == TX_SUCCESS)
        {
            caliOffset.TempWhenCali = dev->temperature;
            log_i("Calibration temperature: %0.2f°C", caliOffset.TempWhenCali);
        }
        else
        {
            log_w("Failed to get temperature data");
            caliOffset.TempWhenCali = 40.0f; // 默认温度
        }

        caliOffset.Calibrated = 1;

    } while (gNormDiff > 0.5f || fabsf(caliOffset.gNorm - 9.8f) > 0.5f || gyroDiff[0] > 0.15f ||
             gyroDiff[1] > 0.15f || gyroDiff[2] > 0.15f ||
             fabsf(caliOffset.GyroOffset[0]) > 0.01f || fabsf(caliOffset.GyroOffset[1]) > 0.01f ||
             fabsf(caliOffset.GyroOffset[2]) > 0.01f);

    // 避免除零错误并计算加速度计缩放因子
    if (caliOffset.gNorm > 0.1f)
    {
        caliOffset.AccelScale = 9.81f / caliOffset.gNorm;
        log_i("Accel scale factor: %f (gNorm: %f)", caliOffset.AccelScale, caliOffset.gNorm);
    }
    else
    {
        caliOffset.AccelScale = 1.0f;
        log_e("Invalid gNorm value: %f, using default scale factor", caliOffset.gNorm);
    }

    // 保存校准数据到Flash
    static uint8_t tmpdata[sizeof(BMI088_Cali_Offset_t) + 2] = {0};
    memcpy(tmpdata, &caliOffset, sizeof(BMI088_Cali_Offset_t));
    tmpdata[sizeof(BMI088_Cali_Offset_t) + 1] = 0xAA;

#if defined(STM32H723xx)
    Module_W25Qxx_WriteBuffer(tmpdata, sizeof(tmpdata));
#elif defined(STM32F407xx)
    BSP_FLASH_Write_Buffer(tmpdata, sizeof(tmpdata));
#endif

    // 将校准结果复制到实例中
    memcpy(&dev->BMI088_Cali_Offset, &caliOffset, sizeof(BMI088_Cali_Offset_t));

    // 显示校准结果
    log_i("Gyro offsets: X=%f, Y=%f, Z=%f", caliOffset.GyroOffset[0], caliOffset.GyroOffset[1],
          caliOffset.GyroOffset[2]);

    Module_Rgb_show(LED_Green);
    DWT_Delay(0.5f);
    return status;
}