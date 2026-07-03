#include "motor_damiao.h"
#include "arm_math.h"
#include "bsp_can.h"
#include "bsp_def.h"
#include "module_offline.h"
#include "motor_def.h"
#include "motor_register.h"
#include "user_lib.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "motor_dji"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define DM_P_MIN  -12.5f
#define DM_P_MAX  12.5f
#define DM_V_MIN  -30.0f
#define DM_V_MAX  30.0f
#define DM_KP_MIN 0.0f
#define DM_KP_MAX 500.0f
#define DM_KD_MIN 0.0f
#define DM_KD_MAX 5.0f
#define DM_T_MIN  -10.0f
#define DM_T_MAX  10.0f

#define DM_TWO_PI 6.283185307f

static DM_MOTOR_t *dm_motor_list = NULL;

// 内部函数
void mit_ctrl(DM_MOTOR_t *motor, float pos, float vel, float kp, float kd, float torq)
{
    uint16_t       pos_tmp, vel_tmp, kp_tmp, kd_tmp, tor_tmp;
    CanTxMessage_t txmessage;

    txmessage.hcan = motor->can_dev->hcan;
    txmessage.id   = motor->can_dev->tx_id + DM_MIT_MODE;
    txmessage.len  = 8;

    pos_tmp = float_to_uint(pos, DM_P_MIN, DM_P_MAX, 16);
    vel_tmp = float_to_uint(vel, DM_V_MIN, DM_V_MAX, 12);
    kp_tmp  = float_to_uint(kp, DM_KP_MIN, DM_KP_MAX, 12);
    kd_tmp  = float_to_uint(kd, DM_KD_MIN, DM_KD_MAX, 12);
    tor_tmp = float_to_uint(torq, DM_T_MIN, DM_T_MAX, 12);

    txmessage.buf[0] = (pos_tmp >> 8);
    txmessage.buf[1] = pos_tmp;
    txmessage.buf[2] = (vel_tmp >> 4);
    txmessage.buf[3] = ((vel_tmp & 0xF) << 4) | (kp_tmp >> 8);
    txmessage.buf[4] = kp_tmp;
    txmessage.buf[5] = (kd_tmp >> 4);
    txmessage.buf[6] = ((kd_tmp & 0xF) << 4) | (tor_tmp >> 8);
    txmessage.buf[7] = tor_tmp;

    BSP_CAN_SendMessage(&txmessage);
}

void pos_speed_ctrl(DM_MOTOR_t *motor, float pos_degree, float vel)
{
    uint8_t       *pbuf, *vbuf;
    CanTxMessage_t txmessage;

    txmessage.hcan = motor->can_dev->hcan;
    txmessage.id   = motor->can_dev->tx_id + DM_POS_MODE;
    txmessage.len  = 8;

    float pos_rad = deg_to_rad(pos_degree);
    VAL_LIMIT(pos_rad, DM_P_MIN, DM_P_MAX);

    pbuf = (uint8_t *)&pos_rad;
    vbuf = (uint8_t *)&vel;

    txmessage.buf[0] = *pbuf;
    txmessage.buf[1] = *(pbuf + 1);
    txmessage.buf[2] = *(pbuf + 2);
    txmessage.buf[3] = *(pbuf + 3);
    txmessage.buf[4] = *vbuf;
    txmessage.buf[5] = *(vbuf + 1);
    txmessage.buf[6] = *(vbuf + 2);
    txmessage.buf[7] = *(vbuf + 3);

    BSP_CAN_SendMessage(&txmessage);
}

void speed_ctrl(DM_MOTOR_t *motor, float vel)
{

    uint8_t       *vbuf;
    CanTxMessage_t txmessage;

    txmessage.hcan = motor->can_dev->hcan;
    txmessage.id   = motor->can_dev->tx_id + DM_SPD_MODE;
    txmessage.len  = 4;

    vbuf = (uint8_t *)&vel;

    txmessage.buf[0] = *vbuf;
    txmessage.buf[1] = *(vbuf + 1);
    txmessage.buf[2] = *(vbuf + 2);
    txmessage.buf[3] = *(vbuf + 3);

    BSP_CAN_SendMessage(&txmessage);
}

void psi_ctrl(DM_MOTOR_t *motor, float pos, float vel, float current)
{
    uint8_t       *pbuf, *vbuf, *ibuf;
    CanTxMessage_t txmessage;

    txmessage.hcan = motor->can_dev->hcan;
    txmessage.id   = motor->can_dev->tx_id + DM_PSI_MODE;
    txmessage.len  = 8;

    uint16_t u16_vel = vel * 100;
    uint16_t u16_cur = current * 10000;

    pbuf = (uint8_t *)&pos;
    vbuf = (uint8_t *)&u16_vel;
    ibuf = (uint8_t *)&u16_cur;

    txmessage.buf[0] = *pbuf;
    txmessage.buf[1] = *(pbuf + 1);
    txmessage.buf[2] = *(pbuf + 2);
    txmessage.buf[3] = *(pbuf + 3);
    txmessage.buf[4] = *vbuf;
    txmessage.buf[5] = *(vbuf + 1);
    txmessage.buf[6] = *ibuf;
    txmessage.buf[7] = *(ibuf + 1);

    BSP_CAN_SendMessage(&txmessage);
}

// 对外接口
void Motor_DM_Cmd(DM_MOTOR_t *motor, DMMotor_Mode_e cmd)
{
    CanTxMessage_t txmessage;

    txmessage.hcan = motor->can_dev->hcan;
    txmessage.id   = motor->can_dev->tx_id + motor->DMMotor_Mode_type;
    txmessage.len  = 8;

    txmessage.buf[0] = 0xff;
    txmessage.buf[1] = 0xff;
    txmessage.buf[2] = 0xff;
    txmessage.buf[3] = 0xff;
    txmessage.buf[4] = 0xff;
    txmessage.buf[5] = 0xff;
    txmessage.buf[6] = 0xff;
    txmessage.buf[7] = (uint8_t)cmd; // 最后一位是命令id

    BSP_CAN_SendMessage(&txmessage);
}

DM_MOTOR_t *Motor_DM_Init(Motor_Init_Config_s *config, uint32_t DM_Mode_type)
{
    // 分配电机结构体内存
    DM_MOTOR_t *DMMotor = NULL;
    BSP_MEM_ALLOC_WAIT(DMMotor, sizeof(DM_MOTOR_t), TX_NO_WAIT);
    if (DMMotor == NULL)
    {
        log_e("Failed to allocate memory for DM motor\n");
        return NULL;
    }
    memset(DMMotor, 0, sizeof(DM_MOTOR_t));

    // motor basic setting 电机基本设置
    DMMotor->motor_info        = config->Motor_init_Info;     // 电机信息
    DMMotor->motor_setting     = config->setting_init_config; // 正反转,闭环类型等
    DMMotor->DMMotor_Mode_type = DM_Mode_type;

    // CAN 设备初始化配置
    Can_Device_Init_Config_s can_config = {
        .hcan  = config->can_device_init_config.hcan,
        .tx_id = config->can_device_init_config.tx_id,
        .rx_id = config->can_device_init_config.rx_id,
    };
    // 注册 CAN 设备并获取引用
    DMMotor->can_dev = BSP_CAN_Device_Init(&can_config);
    if (DMMotor->can_dev == NULL)
    {
        log_e("Failed to initialize CAN device for DM motor");
        BSP_MEM_FREE(DMMotor);
        return NULL;
    }

    // motor controller init 电机控制器初始化
    if (DMMotor->motor_setting.algorithm_type == CONTROL_PID)
    {
        PIDInit(&DMMotor->motor_controller.speed_PID, &config->controller_init_config.speed_PID);
        PIDInit(&DMMotor->motor_controller.angle_PID, &config->controller_init_config.angle_PID);
    }
    else if (DMMotor->motor_setting.algorithm_type == CONTROL_LQR)
    {
        LQRInit(&DMMotor->motor_controller.lqr, &config->controller_init_config.lqr_init);
    }
    DMMotor->motor_controller.other_angle_feedback_ptr = config->controller_init_config.other_angle_feedback_ptr;
    DMMotor->motor_controller.other_speed_feedback_ptr = config->controller_init_config.other_speed_feedback_ptr;

    // 掉线检测
    DMMotor->offline_dev = Module_Offline_device_register(&config->offline_init_conifig);
    // 清除错误
    DMMotor->measure.Error_Code = DM_NO_ERROR;
    Motor_DM_Cmd(DMMotor, DM_CMD_CLEAR_ERROR);
    // 使能电机
    Motor_DM_Cmd(DMMotor, DM_CMD_MOTOR_START);

    // 添加到电机链表
    DMMotor->next = dm_motor_list; // 将当前电机节点插入到链表头部
    dm_motor_list = DMMotor;

    // 注册到电机查找表
    MotorRegistryRegister(DMMotor, DMMotor->can_dev->eventflag, DMMotor->motor_info.motor_type);

    log_i("DM motor initialized");

    return DMMotor;
}

void Motor_DM_Start(DM_MOTOR_t *motor) { motor->motor_setting.enableflag = 1; }

void Motor_DM_Stop(DM_MOTOR_t *motor) { motor->motor_setting.enableflag = 0; }

void Motor_DM_ChangeFeed(DM_MOTOR_t *motor, Closeloop_Type_e loop, uint8_t feedback_source)
{
    if (loop == ANGLE_LOOP)
        motor->motor_setting.angle_feedback_source = feedback_source;
    else if (loop == SPEED_LOOP)
        motor->motor_setting.speed_feedback_source = feedback_source;
    else
        log_e("loop type error, check and func param\n");
}

void Motor_DM_OuterLoop(DM_MOTOR_t *motor, Closeloop_Type_e type) { motor->motor_setting.loop_type = type; }

void Motor_DM_SetRef(DM_MOTOR_t *motor, float ref) { motor->motor_controller.ref = ref; }

static void CalculatePIDOutput(DM_MOTOR_t *motor)
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
        else
            pid_measure = motor->measure.total_angle;

        if (motor->motor_setting.feedback_reverse_flag == 1) pid_measure *= -1;
        // 更新pid_ref进入下一个环
        pid_ref = PIDCalculate(&motor->motor_controller.angle_PID, pid_measure, pid_ref);
    }
    // 计算速度环,(外层闭环为速度或位置)且(启用速度环)时会计算速度环
    if ((motor->motor_setting.loop_type & SPEED_LOOP))
    {
        if (motor->motor_setting.speed_feedback_source == 1)
            pid_measure = *motor->motor_controller.other_speed_feedback_ptr;
        else // MOTOR_FEED
            pid_measure = motor->measure.velocity;

        if (motor->motor_setting.feedback_reverse_flag == 1) pid_measure *= -1;
        // 更新pid_ref
        pid_ref = PIDCalculate(&motor->motor_controller.speed_PID, pid_measure, pid_ref);
    }

    // 力矩范围检查 (安全限制)
    VAL_LIMIT(pid_ref, -motor->motor_info.max_torque, motor->motor_info.max_torque);
    // 输出扭矩
    motor->motor_controller.output_torque = pid_ref;

    motor->motor_controller.output = pid_ref;
}
// clang-format off
static void CalculateLQROutput(DM_MOTOR_t *motor)
{
    float ref = 0, rad_angle = 0, rad_speed = 0;

    ref = motor->motor_controller.ref;
    if (motor->motor_setting.motor_reverse_flag == 1)
    {
        ref *= -1;
    }

    // 角度（弧度）
    rad_angle = (motor->motor_setting.angle_feedback_source == 1)? *motor->motor_controller.other_angle_feedback_ptr: motor->measure.total_angle;
    if (motor->motor_setting.feedback_reverse_flag == 1)
    {
        rad_angle *= -1;
    }
    // 角速度（rad/s）
    rad_speed = (motor->motor_setting.speed_feedback_source == 1)? *motor->motor_controller.other_speed_feedback_ptr: motor->measure.velocity;
    if (motor->motor_setting.feedback_reverse_flag == 1)
    {
        rad_speed *= -1;
    }

    float torque = LQRCalculate(&motor->motor_controller.lqr, rad_angle, rad_speed, ref);

    // 力矩范围检查 (安全限制)
    VAL_LIMIT(torque, -motor->motor_info.max_torque, motor->motor_info.max_torque);

    // 输出扭矩
    motor->motor_controller.output_torque = torque;

    motor->motor_controller.output = torque;
}
// clang-format on

void Motor_DM_Control(void)
{
    DM_MOTOR_t *motor_node = dm_motor_list;

    while (motor_node != NULL)
    {
        if (Module_Offline_get_device_status(motor_node->offline_dev) == STATE_OFFLINE || motor_node->motor_setting.enableflag == 0)
        {
            motor_node->motor_controller.output = 0;
            // 当电机停止或离线时，将PID控制器输出设为0
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

        // 根据电机模式调用对应的控制函数
        switch (motor_node->DMMotor_Mode_type)
        {
        case DM_MIT_MODE:
            mit_ctrl(motor_node, 0, 0, 0, 0, motor_node->motor_controller.output);
            break;

        case DM_POS_MODE:
            pos_speed_ctrl(
                motor_node, motor_node->motor_controller.ref,
                (Module_Offline_get_device_status(motor_node->offline_dev) == STATE_OFFLINE || motor_node->motor_setting.enableflag == 0) ? 0 : PI);
            break;

        case DM_SPD_MODE:
            speed_ctrl(motor_node, motor_node->motor_controller.ref);
            break;

        case DM_PSI_MODE:
            psi_ctrl(motor_node, 0, 0, motor_node->motor_controller.ref);
            break;
        }

        motor_node = motor_node->next; // 移动到链表下一个节点
    }
}

void Motor_DM_Decode(DM_MOTOR_t *motor)
{

    if (motor == NULL || motor->can_dev == NULL)
    {
        return;
    }

    uint16_t tmp; // 用于暂存解析值,稍后转换成float数据

    // 解析错误码
    uint8_t error_code                = (motor->can_dev->rx_buf[0] >> 4) & 0x0F;
    motor->measure.id                 = motor->can_dev->rx_buf[0] & 0x0F;
    motor->measure.Error_Code         = (error_code >= 0x08 && error_code <= 0x0E) ? (DMMotorError_t)error_code : DM_NO_ERROR;
    tmp                               = (uint16_t)((motor->can_dev->rx_buf[1] << 8) | motor->can_dev->rx_buf[2]);
    motor->measure.angle_single_round = uint_to_float(tmp, DM_P_MIN, DM_P_MAX, 16);
    tmp                               = (uint16_t)((motor->can_dev->rx_buf[3] << 4) | motor->can_dev->rx_buf[4] >> 4);
    motor->measure.velocity           = uint_to_float(tmp, DM_V_MIN, DM_V_MAX, 12);
    tmp                               = (uint16_t)(((motor->can_dev->rx_buf[4] & 0x0f) << 8) | motor->can_dev->rx_buf[5]);
    motor->measure.torque             = uint_to_float(tmp, DM_T_MIN, DM_T_MAX, 12);
    motor->measure.T_Mos              = (float)motor->can_dev->rx_buf[6];
    motor->measure.T_Rotor            = (float)motor->can_dev->rx_buf[7];

    // 计算总角度和圈数
    float current_angle = motor->measure.angle_single_round;
    float diff          = current_angle - motor->measure.last_angle_single_round;

    if (diff < -3.14159f)
    {
        diff += DM_TWO_PI;
        motor->measure.total_round++;
    }
    else if (diff > 3.14159f)
    {
        diff -= DM_TWO_PI;
        motor->measure.total_round--;
    }

    motor->measure.total_angle += diff;
    motor->measure.last_angle_single_round = current_angle;

    // 更新在线状态
    Module_Offline_device_update(motor->offline_dev);
}
