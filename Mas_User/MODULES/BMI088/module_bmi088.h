/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-11 13:43:14
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-22 18:46:15
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/BMI088/module_bmi088.h
 * @Description:
 */
#ifndef _BMI088_H_
#define _BMI088_H_

#include "BMI088_reg.h"
#include "bsp_pwm.h"
#include "bsp_spi.h"
#include "pid.h"
#include <stdint.h>

#define BMI088_TEMP_ENABLE 0     // 启用BMI088模块的温度控制
#define BMI088_TEMP_SET    35.0f // BMI088的设定温度

typedef struct
{
    // 标定数据
    float   AccelScale;
    float   GyroOffset[3];
    float   gNorm;        // 重力加速度模长,从标定获取
    float   TempWhenCali; // 标定时温度
    uint8_t Calibrated;
} BMI088_Cali_Offset_t;

typedef struct
{
    uint8_t              initialized;
    SPI_Device          *gyro_device;
    SPI_Device          *acc_device;
    PWM_Device          *bmi088_pwm;
    PIDInstance          pid_temp;
    float                acc[3];
    float                gyro[3];
    float                temperature;
    BMI088_Cali_Offset_t BMI088_Cali_Offset;
    BMI088_ERORR_CODE_e  BMI088_ERORR_CODE;
} Bmi088_device_t;

/**
 * @description: 初始化BMI088
 * @return {UINT},获取成功返回TX_SUCCESS，否则TX_DELETED
 */
UINT Module_BMI088_init();
/**
 * @description: 获取bmi088_device指针
 * @return {Bmi088_device_t *}，返回bmi088_device设备指针
 */
Bmi088_device_t * Module_BMI088_get_device();
/**
 * @description: 获取BMI088加速度数据
 * @return {UINT},获取成功返回TX_SUCCESS，否则TX_DELETED
 */
UINT Module_BMI088_get_accel();
/**
 * @description: 获取BMI088陀螺仪数据
 * @return {UINT},获取成功返回TX_SUCCESS，否则TX_DELETED
 */
UINT Module_BMI088_get_gyro();
/**
 * @description: 获取BMI088温度数据
 * @return {UINT},获取成功返回TX_SUCCESS，否则TX_DELETED
 */
UINT Module_BMI088_get_temp();
/**
 * @description: BMI088温度控制
 * @return {*}
 */
void Module_BMI088_temp_ctrl();
/**
 * @description: BMI088零飘标定
 * @param {BMI088_Instance_t*},BMI088实例指针
 * @return {UINT},获取成功返回TX_SUCCESS，否则TX_DELETED
 */
UINT Module_BMI088_calibrate_bmi088_offset(Bmi088_device_t *dev);

#endif // _BMI088_H_