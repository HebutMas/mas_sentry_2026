#include "motor_dji.h"
#include "bsp_can.h"
#include "bsp_def.h"
#include "module_offline.h"
#include "motor_def.h"
#include "motor_register.h"
#include "power_control.h"
#include "user_lib.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "motor_dji"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define ECD_ANGLE_COEF_DJI     0.043945f // (360/8192),将编码器值转化为角度制
#define ECD_ANGLE_COEF_DJI_RAD 0.000767f // (2.0f * PI / 8192.0f) 将编码器值转化为弧度

// STM32H7 系列
#if defined(STM32H723xx)
#define MOTOR_SENDER_SIZE   15
#define HANDLE_CAN1         &hfdcan1
#define HANDLE_CAN2         &hfdcan2
#define HANDLE_CAN3         &hfdcan3
// 句柄比较辅助宏
#define STR_CAN1            "CAN1"
#define STR_CAN2            "CAN2"
#define STR_CAN3            "CAN3"
#define IS_CAN1_HANDLE(ptr) ((ptr) == HANDLE_CAN1)
#define IS_CAN2_HANDLE(ptr) ((ptr) == HANDLE_CAN2)
#define IS_CAN3_HANDLE(ptr) ((ptr) == HANDLE_CAN3)
// STM32F4 系列
#elif defined(STM32F407xx)
#define MOTOR_SENDER_SIZE   10
#define HANDLE_CAN1         &hcan1
#define HANDLE_CAN2         &hcan2
#define HANDLE_CAN3         NULL // F4 没有 CAN3
// 句柄比较辅助宏
#define STR_CAN1            "CAN1"
#define STR_CAN2            "CAN2"
#define STR_CAN3            "Unknown"
#define IS_CAN1_HANDLE(ptr) ((ptr) == HANDLE_CAN1)
#define IS_CAN2_HANDLE(ptr) ((ptr) == HANDLE_CAN2)
#define IS_CAN3_HANDLE(ptr) (0)
#endif

static DJI_Motor_t *dji_motor_list = NULL;

/**
 * @brief DJI电机发送缓存初始化
 *        C610(m2006)/C620(m3508):0x1ff,0x200;
 *        GM6020:0x1ff,0x2ff 0x1fe 0x2fe
 */
// clang-format off
static CanTxMessage_t sender_assignment[MOTOR_SENDER_SIZE] =
{
    [0]  ={.hcan = HANDLE_CAN1, .id = 0x1ff, .len = 8, .buf = {0},},
    [1]  ={.hcan = HANDLE_CAN1, .id = 0x200, .len = 8, .buf = {0},},
    [2]  ={.hcan = HANDLE_CAN1, .id = 0x2ff, .len = 8, .buf = {0},},
    [3]  ={.hcan = HANDLE_CAN1, .id = 0x1fe, .len = 8, .buf = {0},},
    [4]  ={.hcan = HANDLE_CAN1, .id = 0x2fe, .len = 8, .buf = {0},},
    [5]  ={.hcan = HANDLE_CAN2, .id = 0x1ff, .len = 8, .buf = {0},},
    [6]  ={.hcan = HANDLE_CAN2, .id = 0x200, .len = 8, .buf = {0},},
    [7]  ={.hcan = HANDLE_CAN2, .id = 0x2ff, .len = 8, .buf = {0},},
    [8]  ={.hcan = HANDLE_CAN2, .id = 0x1fe, .len = 8, .buf = {0},},
    [9]  ={.hcan = HANDLE_CAN2, .id = 0x2fe, .len = 8, .buf = {0},},

    /* --- 配置 CAN3 (Index 10-14, 仅 H7) --- */
    #if defined(STM32H723xx)
    [10]  ={.hcan = HANDLE_CAN3, .id = 0x1ff, .len = 8, .buf = {0},},
    [11]  ={.hcan = HANDLE_CAN3, .id = 0x200, .len = 8, .buf = {0},},
    [12]  ={.hcan = HANDLE_CAN3, .id = 0x2ff, .len = 8, .buf = {0},},
    [13]  ={.hcan = HANDLE_CAN3, .id = 0x1fe, .len = 8, .buf = {0},},
    [14]  ={.hcan = HANDLE_CAN3, .id = 0x2fe, .len = 8, .buf = {0},},
    #endif
};

static uint8_t sender_enable_flag[MOTOR_SENDER_SIZE] = {0};
// clang-format on

/**
 * @brief 根据电调/拨码开关上的ID,根据说明书的默认id分配方式计算发送ID和接收ID,
 *        并对电机进行分组以便处理多电机控制命令
 */
static UINT MotorSenderGrouping(DJI_Motor_t *motor, Can_Device_Init_Config_s *config)
{
    uint8_t motor_id = config->tx_id - 1; // 下标从零开始,先减一方便赋值
    uint8_t motor_send_num;
    uint8_t motor_grouping;

    DJI_Motor_t *motor_node = dji_motor_list;
    UINT         status     = TX_SUCCESS;

    switch (motor->motor_info.motor_type)
    {
    case M2006:
    case M3508:
        // 计算分组发送ID
        if (motor_id < 4) // 根据ID分组，每路CAN支持4个电机
        {
            motor_send_num = motor_id; // 发送缓冲区位置

            // 根据CAN句柄确定分组
            if (IS_CAN1_HANDLE(config->hcan))
                motor_grouping = 1;
            else if (IS_CAN2_HANDLE(config->hcan))
                motor_grouping = 6;
            else if (IS_CAN3_HANDLE(config->hcan))
                motor_grouping = 11;
            else
            {
                status = TX_DELETED;
                break;
            }
        }
        else
        {
            motor_send_num = motor_id - 4; // 发送缓冲区位置

            if (IS_CAN1_HANDLE(config->hcan))
                motor_grouping = 0;
            else if (IS_CAN2_HANDLE(config->hcan))
                motor_grouping = 5;
            else if (IS_CAN3_HANDLE(config->hcan))
                motor_grouping = 10;
            else
            {
                status = TX_DELETED;
                break;
            }
        }

        // 计算接收id并设置分组发送id
        config->rx_id = 0x200 + motor_id + 1; // 把ID+1,进行分组设置

        // 设置发送标志位,防止发送空帧
        sender_enable_flag[motor_grouping] = 1;

        motor->message_num  = motor_send_num;
        motor->sender_group = motor_grouping;

        // ID冲突检查：遍历链表检查是否有冲突
        while (motor_node != NULL)
        {
            if (motor_node->can_dev != NULL && motor_node->can_dev->hcan == config->hcan && motor_node->can_dev->rx_id == config->rx_id)
            {
                log_e("ID conflict check failed.ID: [%d],CAN bus: [%s]", config->rx_id,
                      (IS_CAN1_HANDLE(config->hcan)   ? STR_CAN1
                       : IS_CAN2_HANDLE(config->hcan) ? STR_CAN2
                                                      : STR_CAN3));
                status = TX_DELETED;
                break;
            }
            motor_node = motor_node->next;
        }
        break;

    case GM6020_CURRENT:
        if (motor_id < 4)
        {
            motor_send_num = motor_id;

            if (IS_CAN1_HANDLE(config->hcan))
                motor_grouping = 3;
            else if (IS_CAN2_HANDLE(config->hcan))
                motor_grouping = 8;
            else if (IS_CAN3_HANDLE(config->hcan))
                motor_grouping = 13;
            else
            {
                status = TX_DELETED;
                break;
            }
        }
        else
        {
            motor_send_num = motor_id - 4;

            if (IS_CAN1_HANDLE(config->hcan))
                motor_grouping = 4;
            else if (IS_CAN2_HANDLE(config->hcan))
                motor_grouping = 9;
            else if (IS_CAN3_HANDLE(config->hcan))
                motor_grouping = 14;
            else
            {
                status = TX_DELETED;
                break;
            }
        }

        config->rx_id = 0x204 + motor_id + 1; // 把ID+1,进行分组设置

        sender_enable_flag[motor_grouping] = 1;

        motor->message_num  = motor_send_num;
        motor->sender_group = motor_grouping;

        // ID冲突检查
        while (motor_node != NULL)
        {
            if (motor_node->can_dev != NULL && motor_node->can_dev->hcan == config->hcan && motor_node->can_dev->rx_id == config->rx_id)
            {
                log_e("ID conflict check failed.ID: [%d],CAN bus: [%s]", config->rx_id,
                      (IS_CAN1_HANDLE(config->hcan)   ? STR_CAN1
                       : IS_CAN2_HANDLE(config->hcan) ? STR_CAN2
                                                      : STR_CAN3));
                status = TX_DELETED;
                break;
            }
            motor_node = motor_node->next;
        }
        break;

    case GM6020_VOLTAGE:
        if (motor_id < 4)
        {
            motor_send_num = motor_id;

            if (IS_CAN1_HANDLE(config->hcan))
                motor_grouping = 0;
            else if (IS_CAN2_HANDLE(config->hcan))
                motor_grouping = 5;
            else if (IS_CAN3_HANDLE(config->hcan))
                motor_grouping = 10;
            else
            {
                status = TX_DELETED;
                break;
            }
        }
        else
        {
            motor_send_num = motor_id - 4;

            if (IS_CAN1_HANDLE(config->hcan))
                motor_grouping = 2;
            else if (IS_CAN2_HANDLE(config->hcan))
                motor_grouping = 7;
            else if (IS_CAN3_HANDLE(config->hcan))
                motor_grouping = 12;
            else
            {
                status = TX_DELETED;
                break;
            }
        }

        config->rx_id = 0x204 + motor_id + 1; // 把ID+1,进行分组设置

        sender_enable_flag[motor_grouping] = 1;

        motor->message_num  = motor_send_num;
        motor->sender_group = motor_grouping;

        // ID冲突检查
        while (motor_node != NULL)
        {
            if (motor_node->can_dev != NULL && motor_node->can_dev->hcan == config->hcan && motor_node->can_dev->rx_id == config->rx_id)
            {
                log_e("ID conflict check failed.ID: [%d],CAN bus: [%s]", config->rx_id,
                      (IS_CAN1_HANDLE(config->hcan)   ? STR_CAN1
                       : IS_CAN2_HANDLE(config->hcan) ? STR_CAN2
                                                      : STR_CAN3));
                status = TX_DELETED;
                break;
            }
            motor_node = motor_node->next;
        }
        break;
    default:
        break;
    }
    return status;
}

// 电机初始化,返回一个电机实例
DJI_Motor_t *Motor_DJI_Init(Motor_Init_Config_s *config)
{
    // 分配电机结构体内存
    DJI_Motor_t *DJIMotor = NULL;
    BSP_MEM_ALLOC_WAIT(DJIMotor, sizeof(DJI_Motor_t), TX_NO_WAIT);
    if (DJIMotor == NULL)
    {
        log_e("Failed to allocate memory for DJI motor\n");
        return NULL;
    }
    memset(DJIMotor, 0, sizeof(DJI_Motor_t));

    // 电机基本设置
    DJIMotor->motor_info    = config->Motor_init_Info;     // 电机信息
    DJIMotor->motor_setting = config->setting_init_config; // 正反转,闭环类型等

    // 电机分组,因为至多4个电机可以共用一帧CAN控制报文
    if (MotorSenderGrouping(DJIMotor, &config->can_device_init_config) != TX_SUCCESS)
    {
        log_e("Motor Sender Grouping Failed!");
        BSP_MEM_FREE(DJIMotor);
        return NULL;
    }
    // CAN 设备初始化配置
    Can_Device_Init_Config_s can_config = {
        .hcan  = config->can_device_init_config.hcan,
        .tx_id = config->can_device_init_config.tx_id,
        .rx_id = config->can_device_init_config.rx_id,
    };
    // 注册 CAN 设备并获取引用
    DJIMotor->can_dev = BSP_CAN_Device_Init(&can_config);
    if (DJIMotor->can_dev == NULL)
    {
        log_e("Failed to initialize CAN device for DJI motor");
        BSP_MEM_FREE(DJIMotor);
        return NULL;
    }

    // motor controller init 电机控制器初始化
    if (DJIMotor->motor_setting.algorithm_type == CONTROL_PID)
    {
        PIDInit(&DJIMotor->motor_controller.speed_PID, &config->controller_init_config.speed_PID);
        PIDInit(&DJIMotor->motor_controller.angle_PID, &config->controller_init_config.angle_PID);
    }
    else if (DJIMotor->motor_setting.algorithm_type == CONTROL_LQR)
    {
        LQRInit(&DJIMotor->motor_controller.lqr, &config->controller_init_config.lqr_init);
    }

    DJIMotor->motor_controller.other_angle_feedback_ptr = config->controller_init_config.other_angle_feedback_ptr;
    DJIMotor->motor_controller.other_speed_feedback_ptr = config->controller_init_config.other_speed_feedback_ptr;

    // 掉线检测初始化
    DJIMotor->offline_dev = Module_Offline_device_register(&config->offline_init_conifig);

    // 添加到电机链表
    DJIMotor->next = dji_motor_list; // 将当前电机节点插入到链表头部
    dji_motor_list = DJIMotor;

    // 注册到电机查找表
    MotorRegistryRegister(DJIMotor, DJIMotor->can_dev->eventflag, DJIMotor->motor_info.motor_type);

    log_i("DJI motor initialized");

    return DJIMotor;
}

void Motor_DJI_Stop(DJI_Motor_t *motor) { motor->motor_setting.enableflag = 0; }

void Motor_DJI_Start(DJI_Motor_t *motor) { motor->motor_setting.enableflag = 1; }
void Motor_DJI_ChangeFeed(DJI_Motor_t *motor, Closeloop_Type_e loop, uint8_t feedback_source)
{
    if (loop == ANGLE_LOOP)
        motor->motor_setting.angle_feedback_source = feedback_source;
    else if (loop == SPEED_LOOP)
        motor->motor_setting.speed_feedback_source = feedback_source;
    else
        log_e("loop type error, check and func param\n");
}

void Motor_DJI_OuterLoop(DJI_Motor_t *motor, Closeloop_Type_e outer_loop)
{
    // 更新外环类型
    motor->motor_setting.loop_type = outer_loop;
}

// 设置参考值
void Motor_DJI_SetRef(DJI_Motor_t *motor, float ref) { motor->motor_controller.ref = ref; }

static void CalculatePIDOutput(DJI_Motor_t *motor)
{
    float pid_measure, pid_ref;

    pid_ref = motor->motor_controller.ref;
    if (motor->motor_setting.motor_reverse_flag == 1)
    {
        pid_ref *= -1;
    }

    // pid_ref会顺次通过被启用的闭环充当数据的载体
    // 计算位置环,只有启用位置环且外层闭环为位置时会计算速度环输出
    if (motor->motor_setting.loop_type & ANGLE_LOOP)
    {
        if (motor->motor_setting.angle_feedback_source == 1)
            pid_measure = *motor->motor_controller.other_angle_feedback_ptr;
        else // MOTOR
            pid_measure = motor->measure.total_angle;

        if (motor->motor_setting.feedback_reverse_flag == 1) pid_measure *= -1;
        // 更新pid_ref进入下一个环
        pid_ref = PIDCalculate(&motor->motor_controller.angle_PID, pid_measure, pid_ref);
    }
    // 检查是否启用了速度控制环（可以是纯速度控制或位置速度双环）
    if ((motor->motor_setting.loop_type & SPEED_LOOP))
    {
        if (motor->motor_setting.speed_feedback_source == 1)
            pid_measure = *motor->motor_controller.other_speed_feedback_ptr;
        else // MOTOR
            pid_measure = motor->measure.speed_rpm;

        if (motor->motor_setting.feedback_reverse_flag == 1) pid_measure *= -1;
        // 更新pid_ref
        pid_ref = PIDCalculate(&motor->motor_controller.speed_PID, pid_measure, pid_ref);
    }

    // 力矩范围检查 (安全限制)
    VAL_LIMIT(pid_ref, -motor->motor_info.max_torque, motor->motor_info.max_torque);

    // 输出扭矩
    motor->motor_controller.output_torque = pid_ref;

    switch (motor->motor_info.motor_type)
    {
    case GM6020_CURRENT:
    {
        motor->motor_controller.output =
            currentToInteger(-3.0f, 3.0f, -16384, 16384, (pid_ref / (motor->motor_info.torque_constant * motor->motor_info.gear_ratio)));
        break;
    }
    case M3508:
    {
        motor->motor_controller.output =
            currentToInteger(-20.0f, 20.0f, -16384, 16384, (pid_ref / (motor->motor_info.torque_constant * motor->motor_info.gear_ratio)));
        break;
    }
    case M2006:
    {
        motor->motor_controller.output =
            currentToInteger(-10.0f, 10.0f, -10000, 10000, (pid_ref / (motor->motor_info.torque_constant * motor->motor_info.gear_ratio)));
        break;
    }
    default:
        motor->motor_controller.output = 0;
        break;
    }
}

static void CalculateLQROutput(DJI_Motor_t *motor)
{
    float ref = 0, rad_angle = 0, rad_speed = 0;

    ref = motor->motor_controller.ref;
    if (motor->motor_setting.motor_reverse_flag == 1)
    {
        ref *= -1;
    }

    // 角度（rad）
    rad_angle = (motor->motor_setting.angle_feedback_source == 1) ? *motor->motor_controller.other_angle_feedback_ptr : motor->measure.total_angle;
    if (motor->motor_setting.feedback_reverse_flag == 1)
    {
        rad_angle *= -1;
    }

    // 角速度（rad/s）
    rad_speed = (motor->motor_setting.speed_feedback_source == 1) ? *motor->motor_controller.other_speed_feedback_ptr : motor->measure.speed_rad;
    if (motor->motor_setting.feedback_reverse_flag == 1)
    {
        rad_speed *= -1;
    }

    float torque = LQRCalculate(&motor->motor_controller.lqr, rad_angle, rad_speed, ref);

    // 力矩范围检查 (安全限制)
    VAL_LIMIT(torque, -motor->motor_info.max_torque, motor->motor_info.max_torque);
    // 输出扭矩
    motor->motor_controller.output_torque = torque;

    switch (motor->motor_info.motor_type)
    {
    case GM6020_CURRENT:
    {
        motor->motor_controller.output =
            currentToInteger(-3.0f, 3.0f, -16384, 16384, (torque / (motor->motor_info.torque_constant * motor->motor_info.gear_ratio)));
        break;
    }
    case M3508:
    {
        motor->motor_controller.output =
            currentToInteger(-20.0f, 20.0f, -16384, 16384, (torque / (motor->motor_info.torque_constant * motor->motor_info.gear_ratio)));
        break;
    }
    case M2006:
    {
        motor->motor_controller.output =
            currentToInteger(-10.0f, 10.0f, -10000, 10000, (torque / (motor->motor_info.torque_constant * motor->motor_info.gear_ratio)));
        break;
    }
    default:
        motor->motor_controller.output = 0;
        break;
    }
}

void Motor_DJI_Control(void)
{
    uint8_t group, num;

    // 遍历所有电机，计算输出
    DJI_Motor_t *motor_node = dji_motor_list;
    while (motor_node != NULL)
    {
        if (Module_Offline_get_device_status(motor_node->offline_dev) == STATE_OFFLINE || motor_node->motor_setting.enableflag == 0)
        {
            motor_node->motor_controller.output           = 0;
            motor_node->motor_controller.speed_PID.Output = 0;
            motor_node->motor_controller.speed_PID.Iout   = 0;
            motor_node->motor_controller.angle_PID.Output = 0;
            motor_node->motor_controller.angle_PID.Iout   = 0;
        }
        else
        {
            switch (motor_node->motor_setting.algorithm_type)
            {
            case CONTROL_PID:
                CalculatePIDOutput(motor_node);
                break;
            case CONTROL_LQR:
                CalculateLQROutput(motor_node);
                break;
            }
        }
        motor_node = motor_node->next;
    }

    // 执行功率限制，如果有注册的功率控制的电机
    PowerControl_Update();

    // 将限制后的输出填充发送缓冲区
    motor_node = dji_motor_list;
    while (motor_node != NULL)
    {
        group                                     = motor_node->sender_group;
        num                                       = motor_node->message_num;
        sender_assignment[group].buf[2 * num]     = (uint8_t)((int16_t)motor_node->motor_controller.output >> 8);
        sender_assignment[group].buf[2 * num + 1] = (uint8_t)((int16_t)motor_node->motor_controller.output & 0x00ff);
        motor_node                                = motor_node->next;
    }

    // 发送 CAN 消息
    for (size_t i = 0; i < MOTOR_SENDER_SIZE; ++i)
    {
        if (sender_enable_flag[i])
        {
            BSP_CAN_SendMessage(&sender_assignment[i]);
        }
    }
}

void Motor_DJI_Decode(DJI_Motor_t *motor)
{
    if (motor == NULL || motor->can_dev == NULL)
    {
        return;
    }
    // 更新电机数据
    motor->measure.last_ecd           = motor->measure.ecd;
    motor->measure.ecd                = ((uint16_t)motor->can_dev->rx_buf[0] << 8) | motor->can_dev->rx_buf[1];
    motor->measure.angle_single_round = ECD_ANGLE_COEF_DJI_RAD * (float)motor->measure.ecd;
    motor->measure.speed_rpm          = (int16_t)(motor->can_dev->rx_buf[2] << 8 | motor->can_dev->rx_buf[3]);
    motor->measure.speed_rad          = motor->measure.speed_rpm * RPM_2_RAD_PER_SEC;
    motor->measure.real_current       = (int16_t)(motor->can_dev->rx_buf[4] << 8 | motor->can_dev->rx_buf[5]);
    motor->measure.temperature        = motor->can_dev->rx_buf[6];
    // 多圈角度计算
    int16_t delta_ecd = motor->measure.ecd - motor->measure.last_ecd;
    // 处理过零跳变
    if (delta_ecd > 4096)
    {
        motor->measure.total_round--;
    }
    else if (delta_ecd < -4096)
    {
        motor->measure.total_round++;
    }
    // 计算总角度 (单位：弧度)
    motor->measure.total_angle = (float)motor->measure.total_round * (2.0f * PI) + motor->measure.angle_single_round;
    // 更新在线状态
    Module_Offline_device_update(motor->offline_dev);
}
