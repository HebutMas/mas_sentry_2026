/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-10-03 10:23:54
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-28 21:49:15
 * @FilePath: /MAS_RM_DJIC/Mas_User/MODULES/MOTOR/DAMIAO/motor_damiao.h
 * @Description:
 */
#ifndef _DAMIAO_H_
#define _DAMIAO_H_

#include "motor_def.h"
#include <stdint.h>

#define DM_MIT_MODE 0x000
#define DM_POS_MODE 0x100
#define DM_SPD_MODE 0x200
#define DM_PSI_MODE 0x300

typedef enum
{
    DM_NO_ERROR               = 0X00,
    OVERVOLTAGE_ERROR         = 0X08,
    UNDERVOLTAGE_ERROR        = 0X09,
    OVERCURRENT_ERROR         = 0X0A,
    MOS_OVERTEMP_ERROR        = 0X0B,
    MOTOR_COIL_OVERTEMP_ERROR = 0X0C,
    COMMUNICATION_LOST_ERROR  = 0X0D,
    OVERLOAD_ERROR            = 0X0E,
} DMMotorError_t;

typedef enum
{
    DM_CMD_MOTOR_START   = 0xfc, // 使能,会响应指令
    DM_CMD_MOTOR_STOP    = 0xfd, // 停止
    DM_CMD_ZERO_POSITION = 0xfe, // 将当前的位置设置为编码器零位
    DM_CMD_CLEAR_ERROR   = 0xfb  // 清除电机错误
} DMMotor_Mode_e;

typedef struct
{
    uint8_t        id;                      // id
    uint8_t        state;                   // 状态
    float          velocity;                // rad/s
    float          last_angle_single_round; // rad
    float          angle_single_round;      // rad
    float          torque;                  // 力矩
    float          T_Mos;                   // mos温度
    float          T_Rotor;                 // 线圈温度
    float          total_angle;             // 总角度,注意方向
    int32_t        total_round;             // 总圈数,注意方向
    DMMotorError_t Error_Code;
} DM_Motor_Measure_s;

typedef struct DM_MOTOR_t
{
    DM_Motor_Measure_s measure;           // 电机测量值
    Motor_Info_s       motor_info;        // 电机参数
    Motor_Controller_s motor_controller;  // 电机控制器
    Motor_Setting_s    motor_setting;     // 电机设置
    uint32_t           DMMotor_Mode_type; // 达妙电机控制模式
    OfflineDevice     *offline_dev;       // 离线设备指针
    Can_Device        *can_dev;           // CAN 设备句柄
    struct DM_MOTOR_t *next;              // 链表下一个节点指针
} DM_MOTOR_t;

/**
 * @description: 达妙电机发送控制命令
 * @param {DM_MOTOR_t} *motor 电机指针
 * @param {DMMotor_Mode_e} cmd 电机命令（启动/停止/清零等）
 * @return {*}
 */
void Motor_DM_Cmd(DM_MOTOR_t *motor, DMMotor_Mode_e cmd);
/**
 * @brief 达妙电机初始化
 * @param config 电机初始化配置结构体指针
 * @param DM_Mode_type 电机工作模式（MIT/POS/SPD/PSI）
 * @return DM_MOTOR_t* 返回达妙电机指针，失败返回NULL
 */
DM_MOTOR_t *Motor_DM_Init(Motor_Init_Config_s *config, uint32_t DM_Mode_type);
/**
 * @description: 达妙电机使能/启动
 * @param {DM_MOTOR_t} *motor 电机指针
 * @return {*}
 */
void Motor_DM_Start(DM_MOTOR_t *motor);
/**
 * @description: 达妙电机停止
 * @param {DM_MOTOR_t} *motor 电机指针
 * @return {*}
 */
void Motor_DM_Stop(DM_MOTOR_t *motor);
/**
 * @description: 达妙电机修改对应闭环的反馈数据源
 * @param {DM_MOTOR_t} *motor 电机指针
 * @param {Closeloop_Type_e} loop 对应的闭环类型
 * @param {uint8_t} feedback_source 反馈数据源类型（0：电机反馈，1：外部反馈）
 * @return {*}
 */
void Motor_DM_ChangeFeed(DM_MOTOR_t *motor, Closeloop_Type_e loop, uint8_t feedback_source);
/**
 * @description: 达妙电机设置外环控制类型
 * @param {DM_MOTOR_t} *motor 电机指针
 * @param {Closeloop_Type_e} closeloop_type 外环控制类型
 * @return {*}
 */
void Motor_DM_OuterLoop(DM_MOTOR_t *motor, Closeloop_Type_e closeloop_type);
/**
 * @description: 达妙电机设置参考值
 * @param {DM_MOTOR_t} *motor 电机指针
 * @param {float} ref 参考值（注意这里是rad（位置）|| rad/s（速度））
 * @return {*}
 */
void Motor_DM_SetRef(DM_MOTOR_t *motor, float ref);
/**
 * @description: 达妙电机控制函数，在电机线程中周期调用
 * @note 达妙官方建议
 * 1. 最好 Master ID 大于 CAN ID && Master ID 各不相同 ，此设置无需插入延时，可有效提高控制频率。
 * 2. 在无法保证 条件1
 的情况下，在发送命令时插入200us以上的延时，也可有效预防总线错误、掉帧以及死机问题。
 * 3. 选好合适的发送控制指令频率：
    - 1个CAN口控制3个电机，发送频率最好选择为1kHz。
    - 1个CAN口控制6个电机，发送频率最好选择为0.5kHz，并且每两数据帧之间插入 200us 延时。
 * @param {*}
 * @return {*}
 */
void Motor_DM_Control(void);
/**
 * @description: 达妙电机数据解析，处理CAN接收到的电机数据
 * @param {DM_MOTOR_t} *motor 电机指针
 * @return {*}
 */
void Motor_DM_Decode(DM_MOTOR_t *motor);

#endif // _DAMIAO_H_