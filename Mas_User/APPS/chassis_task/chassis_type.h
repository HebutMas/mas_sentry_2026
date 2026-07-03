/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-29 03:21:32
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-30 19:31:43
 * @FilePath: /MAS_RM_DJIC/Mas_User/APPS/chassis_task/chassis_type.h
 * @Description:
 */
#ifndef _CHASSIS_TYPE_H_
#define _CHASSIS_TYPE_H_

#include "motor_dji.h"
#include "robot_config.h"

#if CHASSIS_TYPE == 3
/**
 * @brief 舵轮底盘运动学解算
 * @param motors 电机数组 (0-3:驱动电机3508, 4-7:转向电机6020)
 * @param vx     底盘系 X 速度
 * @param vy     底盘系 Y 速度
 * @param vw     旋转速度
 */
void Chassis_Calculate_Target(DJI_Motor_t *motors[8], float vx, float vy, float vw);
#endif

#endif // _CHASSIS_TYPE_H_