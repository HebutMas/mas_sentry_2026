/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-19 08:53:55
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-26 17:51:42
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/BEEP/module_beep.h
 * @Description:
 */
#ifndef _MODULE_BEEP_H_
#define _MODULE_BEEP_H_

#include "tx_port.h"
#include <stdint.h>

/**
 * @brief 蜂鸣器模块初始化
 */
void Module_Beep_init();
/**
 * @description: 蜂鸣器开启
 * @return {*}
 */
void Module_Beep_Start();
/**
 * @description: 蜂鸣器关闭
 * @return {*}
 */
void Module_Beep_Stop();
/**
 * @description: 蜂鸣器音调设置
 * @param {uint16_t} psc，音调
 * @param {uint16_t} pwm，音色
 * @return {*}
 */
void Module_Beep_set(uint16_t psc, uint16_t pwm);

#endif // _MODULE_BEEP_H_