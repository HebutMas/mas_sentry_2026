/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-08-06 10:31:16
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-26 17:52:37
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/BEEP/module_beep.c
 * @Description:
 */

#include "module_beep.h"
#include "bsp_pwm.h"
#include "tim.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "module_beep"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

// 蜂鸣器设备实例
static PWM_Device *beep_pwm_dev = NULL;

void Module_Beep_init()
{
#if defined(STM32H723xx)
    PWM_Init_Config pwm_config = {
        .htim = &htim12, .channel = TIM_CHANNEL_2, .dutyx10 = 0, .Mode = PWM_MODE_BLOCKING};
#elif defined(STM32F407xx)
    PWM_Init_Config pwm_config = {
        .htim = &htim4, .channel = TIM_CHANNEL_3, .dutyx10 = 0, .Mode = PWM_MODE_BLOCKING};
#endif

    beep_pwm_dev = BSP_PWM_Device_Init(&pwm_config);
    if (beep_pwm_dev == NULL)
    {
        return;
    }

    // 默认关闭蜂鸣器
    Module_Beep_Stop();

    log_i("Module BEEP init success");
}

void Module_Beep_Start(){
    BSP_PWM_Start(beep_pwm_dev, NULL, 0);
}

void Module_Beep_Stop(){
    BSP_PWM_Stop(beep_pwm_dev);
}

void Module_Beep_set(uint16_t psc, uint16_t pwm){
    if (beep_pwm_dev == NULL)
    {
        return;
    }

    BSP_PWM_SetPSC(beep_pwm_dev,psc);
    BSP_PWM_SetPulse(beep_pwm_dev,pwm);
}