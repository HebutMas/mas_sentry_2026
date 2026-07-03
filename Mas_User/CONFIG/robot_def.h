/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-02-04 23:23:59
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-26 17:51:49
 * @FilePath: \mas_rm_djic\Mas_User\CONFIG\robot_def.h
 * @Description:
 */
#ifndef _ROBOT_DEF_H_
#define _ROBOT_DEF_H_

#include <stdint.h>

// 这里根据各自机器人修改

#pragma pack(1)

// 云台模式设置
typedef enum
{
    GIMBAL_ZERO_FORCE = 0, // 电流零输入
    GIMBAL_GYRO_MODE,      // 云台陀螺仪反馈模式
    GIMBAL_AUTO_MODE,      // 自瞄导航模式
} gimbal_mode_e;

// cmd发布的云台控制数据,由gimbal订阅
typedef struct
{
    float         yaw;
    float         pitch;
    uint32_t      auto_search;
    gimbal_mode_e gimbal_mode;
} Gimbal_Ctrl_Cmd_s;

typedef struct
{
    float yaw_ecd;
} Gimbal_Upload_Data_s;

// 发射模式设置
typedef enum
{
    SHOOT_OFF = 0,
    SHOOT_ON,
} shoot_mode_e;
typedef enum
{
    FRICTION_OFF = 0, // 摩擦轮关闭
    FRICTION_ON,      // 摩擦轮开启
} friction_mode_e;
typedef enum
{
    LOAD_STOP = 0,  // 停止发射
    LOAD_REVERSE,   // 反转
    LOAD_1_BULLET,  // 单发
    LOAD_BURSTFIRE, // 连发
} loader_mode_e;
// cmd发布的发射控制数据,由shoot订阅
typedef struct
{
    shoot_mode_e    shoot_mode;
    loader_mode_e   load_mode;
    friction_mode_e friction_mode;
} Shoot_Ctrl_Cmd_s;

// 底盘模式设置
typedef enum
{
    CHASSIS_ZERO_FORCE = 0,    // 电流零输入
    CHASSIS_FOLLOW_GIMBAL_YAW, // 底盘跟随云台
    CHASSIS_ROTATE,            // 小陀螺模式
    CHASSIS_ROTATE_REVERSE,    // 小陀螺模式反转
    CHASSIS_AUTOMODE
} chassis_mode_e;

// cmd发布的底盘控制数据,由chassis订阅
typedef struct
{
    // 控制部分
    float          vx;           // 前进方向速度
    float          vy;           // 横移方向速度
    float          wz;           // 旋转速度
    float          offset_angle; // 对齐角度
    chassis_mode_e chassis_mode; // 底盘模式
} Chassis_Ctrl_Cmd_s;

typedef struct
{
    uint8_t  Robot_Color;
    uint16_t projectile_allowance_17mm;       // 剩余发弹量
    uint8_t  power_management_shooter_output; // 功率管理 shooter 输出
    uint16_t current_hp;                      // 机器人当前血量
    uint16_t outpost_HP;                      // 前哨站血量
    uint16_t base_HP;                         // 基地血量
    uint8_t  game_progess;
} Chassis_Upload_Data_s;

typedef struct
{
    chassis_mode_e  chassis;
    friction_mode_e frict;
    float           yaw;
    float           pitch;
    float           offset_angle;
    uint16_t        super_cap_pct;
    int8_t          auto_aim;
} Referee_Send_Ui_s;

#pragma pack()

#endif // _ROBOT_DEF_H_