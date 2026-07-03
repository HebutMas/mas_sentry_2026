/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-03-10 11:33:35
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-10 16:06:04
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\MOTOR\DJI\POWER_CONTROL\power_control.h
 * @Description:
 */
#ifndef _POWER_CONTROL_H_
#define _POWER_CONTROL_H_

#include "motor_def.h"
#include "motor_dji.h"
#include <stdint.h>

// 功率控制支持的最大电机数量
#define POWER_CTRL_MAX_MOTOR_NUM   8

// 舵轮底盘: 舵向轮最大功率占比（驱动轮得剩余）
#define POWER_CTRL_STEER_RATIO     0.8f

/* 误差权重切换阈值（rad/s）
 * sumError < POWER_CTRL_PROP_THRESHOLD  -> 完全按功率比例分配
 * sumError > POWER_CTRL_ERROR_THRESHOLD -> 完全按误差比例分配
 * 中间段线性插值 */
#define POWER_CTRL_ERROR_THRESHOLD 20.0f
#define POWER_CTRL_PROP_THRESHOLD  15.0f

// 电机在功率控制中的角色
typedef enum
{
    PC_ROLE_DRIVE = 0, // 驱动轮: 纳入驱动功率预算，超限时被限制
    PC_ROLE_STEER = 1, // 舵向轮: 预留功率占比并限制，剩余让出给驱动轮
} PowerCtrl_Role_e;

// 单个电机的功率控制参数 
typedef struct
{
    float k1;
    float k2;
    float k3;

    float raw_power;
    float assigned_power;
    float output;
} PowerControl_Param_t;

/**
 * @brief 设置当前帧的功率上限与缓冲能量
 * @param power_limit        裁判系统下发的功率限额 (W)
 * @param buffer_energy      当前缓冲能量 (J)
 * @param buffer_energy_use  是否允许使用缓冲能量（0:不使用 / 1:使用）
 */
void PowerControl_Set(float power_limit, float buffer_energy, uint8_t buffer_energy_use);

/**
 * @brief 注册DJI 电机
 * @param motor      DJI_Motor_t 实例指针
 * @param role       电机角色: PC_ROLE_DRIVE 驱动轮 / PC_ROLE_STEER 舵向轮
 * @param param      该电机的初始损耗模型参数
 */
void PowerControl_Motor_Init(DJI_Motor_t *motor, PowerCtrl_Role_e role, PowerControl_Param_t param);

/**
 * @brief 功率分配计算
 * @note: 需在每次计算完成后、发送报文前调用
 */
void PowerControl_Update(void);

/**
 * @brief 获取指定电机经过功率限制后的输出值
 * @param motor      电机实例指针
 * @param motor_type 电机类型枚举
 * @return 限制后的输出
 */
float PowerControl_Get(DJI_Motor_t *motor, Motor_Type_e motor_type);

#endif // _POWER_CONTROL_H_
