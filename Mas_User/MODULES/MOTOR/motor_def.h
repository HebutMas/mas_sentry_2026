/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-26 01:14:35
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-10 16:00:08
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\MOTOR\motor_def.h
 * @Description:
 */
#ifndef __MOTOR_DEF_H
#define __MOTOR_DEF_H

#include "bsp_can.h"
#include "lqr.h"
#include "module_offline.h"
#include "pid.h"

/* 闭环类型 */
typedef enum
{
    OPEN_LOOP            = 0x0, // 0b0000 - 开环控制
    SPEED_LOOP           = 0x2, // 0b0010 - 速度闭环控制
    ANGLE_LOOP           = 0x4, // 0b0100 - 位置闭环控制
    ANGLE_AND_SPEED_LOOP = 0x6, // 0b0110 - 位置速度双闭环控制
} Closeloop_Type_e;
/* 控制算法类型 */
typedef enum
{
    CONTROL_PID = 0,
    CONTROL_LQR
} Control_Algorithm_Type_e;
/* 电机控制设置,包括闭环类型,反转标志和反馈来源 */
typedef struct
{
    Closeloop_Type_e         loop_type;             // 闭环类型选择
    Control_Algorithm_Type_e algorithm_type;        // 算法类型
    uint8_t                  enableflag;            // 使能标志（0-->电机停止，1-->电机正常控制）
    uint8_t                  motor_reverse_flag;    // 是否反转(0-->正转，1-->反转)
    uint8_t                  feedback_reverse_flag; // 反馈是否反向（0-->反馈方向不变，1-->反馈方向反向）
    uint8_t                  angle_feedback_source; // 角度反馈类型（0-->电机反馈，1-->外部反馈指针）
    uint8_t                  speed_feedback_source; // 速度反馈类型（0-->电机反馈，1-->外部反馈指针）
} Motor_Setting_s;

/* 电机控制器,包括其他来源的反馈数据指针,控制器和电机的参考输入*/
typedef struct
{
    float *other_angle_feedback_ptr; // 其他反馈来源的反馈数据指针
    float *other_speed_feedback_ptr;

    PIDInstance speed_PID; // PID速度环控制器实例
    PIDInstance angle_PID; // PID角度环控制器实例

    LQRInstance lqr; // lqr控制器实例

    float ref;           // 输入
    float output;        // 输出
    float output_torque; // 输出扭矩
} Motor_Controller_s;

/* 电机类型枚举 */
typedef enum
{
    MOTOR_TYPE_NONE = 0,
    GM6020_CURRENT,
    GM6020_VOLTAGE,
    M3508,
    M2006,
    DM4310,
    DM6220
} Motor_Type_e;

/* 电机基本信息结构体，包含电机类型、减速比和扭矩常数等 */
typedef struct
{
    Motor_Type_e motor_type;      // 电机类型
    float        gear_ratio;      // 减速比
    float        torque_constant; // 减速前扭矩常数 (Nm/A)
    float        max_torque;      // 最大力矩 (Nm)
} Motor_Info_s;

/**
 * @brief 电机控制器初始化结构体,包括三环PID的配置以及两个反馈数据来源指针
 *        如果不需要某个控制环,可以不设置对应的pid config
 *        需要其他数据来源进行反馈闭环,不仅要设置这里的指针还需要在Motor_Control_Setting_s启用其他数据来源标志
 */
typedef struct
{
    float *other_angle_feedback_ptr; // 角度反馈数据指针,注意电机使用total_angle
    float *other_speed_feedback_ptr; // 速度反馈数据指针,单位为angle per sec

    PID_Init_Config_s speed_PID;
    PID_Init_Config_s angle_PID;

    LQR_Init_Config_s lqr_init;
} Motor_Controller_Init_s;

/* 用于初始化电机的结构体,各类电机通用 */
typedef struct
{
    Motor_Controller_Init_s  controller_init_config;
    Motor_Setting_s          setting_init_config;
    Motor_Info_s             Motor_init_Info;
    Offline_Init_config_t    offline_init_conifig;
    Can_Device_Init_Config_s can_device_init_config;
} Motor_Init_Config_s;

#endif // MOTOR_DEF_H
