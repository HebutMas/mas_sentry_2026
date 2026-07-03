/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-25 21:05:31
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-25 21:40:58
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/MOTOR/motor_register.h
 * @Description:
 */
#ifndef _MOTOR_REGISTER_H_
#define _MOTOR_REGISTER_H_

#include "motor_def.h"

/**
 * @brief 注册电机到全局查找表
 * @param motor_ptr 电机结构体指针
 * @param eventflag 在 candevice 中分配好的 flag
 * @param type 电机类型
 */
UINT MotorRegistryRegister(void *motor_ptr, uint32_t eventflag, Motor_Type_e type);
/**
 * @brief 根据 eventflag 查找电机指针
 * @note 这个函数与MotorRegistryGetType共同使用
 * @param eventflag 触发的标志位
 * @return void* 找到的电机指针，失败返回 NULL
 */
void *MotorRegistryLookup(uint32_t eventflag);
/**
 * @brief 根据 eventflag 查找电机类型
 * @note 这个函数与MotorRegistryLookup共同使用
 * @param eventflag 触发的标志位
 * @return Motor_Type_e 电机类型
 */
Motor_Type_e MotorRegistryGetType(uint32_t eventflag);


#endif // _MOTOR_REGISTER_H_