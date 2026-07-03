#ifndef _MOTOR_DJI_H_
#define _MOTOR_DJI_H_

#include "motor_def.h"
#include <stdint.h>

/* DJI 电机数据 */
typedef struct
{
    uint16_t last_ecd;           // 上一次读取的编码器值
    uint16_t ecd;                // 0-8191,刻度总共有8192格
    float    angle_single_round; // 单圈角度（rad）
    float    speed_rad;          // (rad/s)
    float    speed_rpm;          // rpm
    int16_t  real_current;       // 实际电流 A
    uint8_t  temperature;        // 温度 Celsius
    float    total_angle;        // 总角度,注意方向(rad)
    int32_t  total_round;        // 总圈数,注意方向
} DJI_Measure_s;

/* DJI 电机结构体 */
typedef struct DJI_Motor_t
{
    Motor_Info_s        motor_info;       // 电机参数
    Motor_Controller_s  motor_controller; // 电机控制器
    Motor_Setting_s     motor_setting;    // 电机设置
    DJI_Measure_s       measure;          // 电机数据
    uint8_t             sender_group;     // 发送组号 0-9
    uint8_t             message_num;      // 组内序号 0-3
    OfflineDevice      *offline_dev;      // 离线设备指针
    Can_Device         *can_dev;          // CAN 设备句柄
    struct DJI_Motor_t *next;             // 链表下一个节点指针
} DJI_Motor_t;

/**

 * @brief DJI 电机初始化
 * @param config Motor_Init_Config_s指针
 * @return DJI_Motor_t* 返回dji电机指针
 */
DJI_Motor_t *Motor_DJI_Init(Motor_Init_Config_s *config);
/**
 * @description: DJI电机停止
 * @param {DJIMotor_t} *motor，电机指针
 * @return {*}
 */
void Motor_DJI_Stop(DJI_Motor_t *motor);
/**
 * @description: DJI电机使能
 * @param {DJIMotor_t} *motor，电机指针
 * @return {*}
 */
void Motor_DJI_Start(DJI_Motor_t *motor);
/**
 * @description: DJI电机修改对应闭环的反馈数据源
 * @param {DJI_Motor_t} *motor，电机指针
 * @param {Closeloop_Type_e} loop，对应的闭环类型
 * @param {feedback_source} type，修改的反馈数据源来源（电机/外部反馈）
 * @return {*}
 */
void Motor_DJI_ChangeFeed(DJI_Motor_t *motor, Closeloop_Type_e loop, uint8_t feedback_source);
/**
 * @description: DJI电机外环修改
 * @param {DJIMotor_t} *motor，电机指针
 * @param {Closeloop_Type_e} outer_loop，外环类型
 * @return {*}
 */
void Motor_DJI_OuterLoop(DJI_Motor_t *motor, Closeloop_Type_e outer_loop);
/**
 * @description: DJI电机设置参考值
 * @param {DJI_Motor_t} *motor
 * @return {*}
 */
void Motor_DJI_SetRef(DJI_Motor_t *motor, float ref);
/**
 * @description: DJI电机控制,在电机线程中调用
 * @param {*}
 * @return {*}
 */
void Motor_DJI_Control(void);
/**
 * @description: DJI电机数据解析
 * @param {DJIMotor_t} *motor，电机指针
 * @return {*}
 */
void Motor_DJI_Decode(DJI_Motor_t *motor);

#endif // _MOTOR_DJI_H_