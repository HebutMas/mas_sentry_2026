/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-17 10:52:48
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-23 14:53:33
 * @FilePath: \mas_rm_djic\Mas_User\CONFIG\robot_config.h
 * @Description:
 */
#ifndef _ROBOT_CONFIG_H_
#define _ROBOT_CONFIG_H_

/* 机器人通讯定义*/

// #define ONE_BOARD // 单板控制

#ifndef ONE_BOARD // 多板控制 （注意只能有一个生效）
#define CHASSIS_BOARD //底盘板
//#define GIMBAL_BOARD // 云台板
// 检查是否出现主控板定义冲突,只允许一个开发板定义存在,否则编译会自动报错
#if (defined(CHASSIS_BOARD) + defined(GIMBAL_BOARD) != 1)
#error Conflict board definition! You can only define one board type.
#endif
#endif

// clang-format off
/* 机器人关键参数定义 ,(这里参数根据机器人实际自行定义) */ // 1 表示开启 0 表示关闭
// 小云台参数
#define SMALL_YAW_ALIGN_ECD           4692            // 小yaw与大yaw保持居中对齐时的编码器角度
#define SMALL_YAW_PITCH_HORIZON_ANGLE 0.0f            // 云台处于水平位置时编码器值,若对云台有机械改动需要修改
#define SMALL_YAW_PITCH_MAX_ANGLE     40.0f           // 云台竖直方向最大角度 (注意反馈如果是陀螺仪，则填写陀螺仪的角度)
#define SMALL_YAW_PITCH_MIN_ANGLE     -20.0f          // 云台竖直方向最小角度 (注意反馈如果是陀螺仪，则填写陀螺仪的角度)
// 云台参数
#define YAW_CHASSIS_ALIGN_ECD         4096            // 云台和底盘对齐指向相同方向时的电机编码器值,若对云台有机械改动需要修改
// 发射参数
#define REDUCTION_RATIO_LOADER         36.0f          // 2006拨盘电机的减速比,英雄需要修改为3508的19.0f
#define ONE_BULLET_DELTA_ANGLE         60.0f          // 发射一发弹丸拨盘转动的距离,由机械设计图纸给出
#define NUM_PER_CIRCLE                 6              // 拨盘一圈的装载量
// 机器人底盘修改的参数,单位为M(米)
#define CHASSIS_TYPE                   3              // 1 麦克纳姆轮底盘 2 全向轮底盘 3 舵轮底盘 4 半舵底盘
#define WHEEL_R                        0.5            // 投影点距离地盘中心为r
#define CENTER_GIMBAL_OFFSET_X         0              // 云台旋转中心距底盘几何中心的距离,前后方向,云台位于正中心时默认设为0
#define CENTER_GIMBAL_OFFSET_Y         0              // 云台旋转中心距底盘几何中心的距离,左右方向,云台位于正中心时默认设为0
#define RADIUS_WHEEL_M                 0.12           // 轮子半径
#define CHASSIS_DECELE_RATIO           16             // 底盘电机的减速比

#if CHASSIS_TYPE == 3
// 这里的值是电机在（正朝前）时的弧度，(编码器数值 * ecd转弧度)
#define CHASSIS_STEER_ALIGN_RAD_LF  (7160 * 0.000767f)  // 左前
#define CHASSIS_STEER_ALIGN_RAD_LB  (1125 * 0.000767f)  // 左后
#define CHASSIS_STEER_ALIGN_RAD_RB  (1694 * 0.000767f)  // 右后
#define CHASSIS_STEER_ALIGN_RAD_RF  (6476 * 0.000767f)  // 右前

// 底盘正方向偏置角（弧度）
// 当正方向从机体前方改为某舵轮方向时，在此填写该舵轮相对机体前方的角度（弧度）
// 例如：右前轮方向为正方向时填 (-PI/4)，即 -0.7854f
// 不修改 ALIGN_RAD 时，通过此值旋转底盘坐标系来对齐新正方向
#define CHASSIS_FORWARD_OFFSET_RAD  (0.0f)
#endif

// clang-format on

#endif // _ROBOT_CONFIG_H_