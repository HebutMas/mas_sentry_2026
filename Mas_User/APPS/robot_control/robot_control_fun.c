/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-10-26 15:51:20
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-04-02 22:18:34
 * @FilePath: \mas_rm_djic\Mas_User\APPS\robot_control\robot_control_fun.c
 * @Description:
 */
#include "robot_control_fun.h"
#include "dt7.h"
#include "module_offline.h"
#include "power_control.h"
#include "referee_protocol.h"
#include "remote_data.h"
#include "robot_config.h"
#include "robot_def.h"
#include "sbus.h"
#include "user_lib.h"
#include "vt03.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief 计算相对于对齐角度的偏差，返回弧度制
 * @param getyawangle 当前编码器值 (0 - 8191)
 * @return 偏差弧度值 (范围 -PI 到 PI)
 */
float CalcOffsetAngle(float getyawangle)
{
    static float offset_ecd;

    // 定义编码器范围常量
    const float ECD_MAX  = 8191.0f; // 编码器最大值
    const float ECD_HALF = 4095.5f; // 编码器中点 (对应180度)

    // 计算原始偏差 (单位: 编码器值)
    offset_ecd = getyawangle - YAW_CHASSIS_ALIGN_ECD;

    // 归一化处理 (使其在 -4095.5 到 4095.5 之间)，即将偏差映射到最近的路径，处理过零点问题
    if (offset_ecd > ECD_HALF)
    {
        offset_ecd -= ECD_MAX;
    }
    else if (offset_ecd < -ECD_HALF)
    {
        offset_ecd += ECD_MAX;
    }

    // 转换为弧度
    return offset_ecd * (2.0f * PI / 8191.0f);
}

void RemoteControlSet(Chassis_Ctrl_Cmd_s *Chassis_Ctrl, Shoot_Ctrl_Cmd_s *Shoot_Ctrl, Gimbal_Ctrl_Cmd_s *Gimbal_Ctrl)
{
    // 检查参数
    if (Chassis_Ctrl == NULL || Shoot_Ctrl == NULL || Gimbal_Ctrl == NULL)
    {
        return;
    }

#if defined(REMOTE_SOURCE)
// 根据遥控器类型处理控制逻辑
#if REMOTE_SOURCE == 1 // SBUS遥控器
    {
        // 获取SBUS遥控器数据
        SBUS_Instance_t *sbus_instance = NULL;
        sbus_instance                  = remote_get_sbus();

        if (Module_Offline_get_device_status(sbus_instance->offline_dev) == STATE_ONLINE && sbus_instance != NULL)
        {
            // 摇杆控制底盘移动
            Chassis_Ctrl->vx = 0.004f * remote_get_sbus_channel(2);
            Chassis_Ctrl->vy = -0.004f * remote_get_sbus_channel(1);

            // 云台控制部分
            int16_t ch6_state = remote_get_sbus_channel(6);
            if (ch6_state == SBUS_CHX_UP)
            {
                Gimbal_Ctrl->gimbal_mode = GIMBAL_GYRO_MODE;
                Gimbal_Ctrl->yaw -= 0.001f * (float)(remote_get_sbus_channel(4));
                Gimbal_Ctrl->pitch += 0.001f * (float)(remote_get_sbus_channel(3));
                VAL_LIMIT(Gimbal_Ctrl->pitch, SMALL_YAW_PITCH_MIN_ANGLE, SMALL_YAW_PITCH_MAX_ANGLE);
            }
            // else if (ch6_state == SBUS_CHX_BIAS)
            // {
            //     Gimbal_Ctrl->gimbal_mode = GIMBAL_ZERO_FORCE;
            // }
            else if (ch6_state == SBUS_CHX_DOWN)
            {
                Gimbal_Ctrl->gimbal_mode   = GIMBAL_AUTO_MODE;
                Chassis_Ctrl->chassis_mode = CHASSIS_AUTOMODE;
            }

            // 发射机构控制部分
            int16_t ch5_state = remote_get_sbus_channel(5);
            if (ch5_state == SBUS_CHX_UP)
            {
                Shoot_Ctrl->shoot_mode    = SHOOT_OFF;
                Shoot_Ctrl->friction_mode = FRICTION_OFF;
                Shoot_Ctrl->load_mode     = LOAD_STOP;
            }
            else if (ch5_state == SBUS_CHX_BIAS)
            {
                Shoot_Ctrl->shoot_mode    = SHOOT_ON;
                Shoot_Ctrl->friction_mode = FRICTION_OFF;
                Shoot_Ctrl->load_mode     = LOAD_STOP;
            }
            else if (ch5_state == SBUS_CHX_DOWN)
            {
                Shoot_Ctrl->shoot_mode    = SHOOT_ON;
                Shoot_Ctrl->friction_mode = FRICTION_ON;

                int16_t ch7_state = remote_get_sbus_channel(7);
                if (ch7_state == SBUS_CHX_BIAS)
                {
                    Shoot_Ctrl->load_mode = LOAD_STOP;
                }
                else if (ch7_state == SBUS_CHX_UP)
                {
                    Shoot_Ctrl->load_mode = LOAD_1_BULLET;
                }
                else if (ch7_state == SBUS_CHX_DOWN)
                {
                    Shoot_Ctrl->load_mode = LOAD_BURSTFIRE;
                }
            }

            // 底盘控制部分
            int16_t ch8_state = remote_get_sbus_channel(8);
            if (ch8_state == SBUS_CHX_UP)
            {
                Chassis_Ctrl->chassis_mode = CHASSIS_FOLLOW_GIMBAL_YAW;
            }
            else if (ch8_state == SBUS_CHX_BIAS)
            {
                Chassis_Ctrl->chassis_mode = CHASSIS_ROTATE;
            }
            else if (ch8_state == SBUS_CHX_DOWN)
            {
                Chassis_Ctrl->chassis_mode = CHASSIS_ROTATE_REVERSE;
            }
        }
        else
        {
            // 遥控器离线处理
            Gimbal_Ctrl->gimbal_mode   = GIMBAL_ZERO_FORCE;
            Chassis_Ctrl->chassis_mode = CHASSIS_ZERO_FORCE;
            Shoot_Ctrl->shoot_mode     = SHOOT_OFF;
            Shoot_Ctrl->friction_mode  = FRICTION_OFF;
            Shoot_Ctrl->load_mode      = LOAD_STOP;
            memset(Chassis_Ctrl, 0, sizeof(Chassis_Ctrl_Cmd_s));
        }
    }
#elif REMOTE_SOURCE == 2 // DT7遥控器
    {
        // 获取DT7遥控器数据
        DT7_Instance_t *dt7_instance = NULL;
        dt7_instance                 = remote_get_dt7();
        if (Module_Offline_get_device_status(dt7_instance->offline_dev) == STATE_ONLINE && dt7_instance != NULL)
        {
            // 摇杆控制底盘移动
            Chassis_Ctrl->vx = -1.0f * remote_get_dt7_channel(2);
            Chassis_Ctrl->vy = 1.0f * remote_get_dt7_channel(1);

            // 云台控制部分
            uint8_t sw1_state = remote_get_dt7_channel(5);
            if (sw1_state == DT7_SW_UP)
            {
                Gimbal_Ctrl->gimbal_mode = GIMBAL_GYRO_MODE;
                Gimbal_Ctrl->yaw -= 0.001f * (float)(remote_get_dt7_channel(3));
                Gimbal_Ctrl->pitch += 0.001f * (float)(remote_get_dt7_channel(4));
                VAL_LIMIT(Gimbal_Ctrl->pitch, SMALL_YAW_PITCH_MIN_ANGLE, SMALL_YAW_PITCH_MAX_ANGLE);
            }
            else if (sw1_state == DT7_SW_DOWN)
            {
                Gimbal_Ctrl->gimbal_mode = GIMBAL_AUTO_MODE;
            }

            // 发射机构控制部分
            if (sw1_state == DT7_SW_UP)
            {
                Shoot_Ctrl->shoot_mode    = SHOOT_ON;
                Shoot_Ctrl->friction_mode = FRICTION_OFF;
                Shoot_Ctrl->load_mode     = LOAD_STOP;
            }
            else if (sw1_state == DT7_SW_MID)
            {
                Shoot_Ctrl->shoot_mode    = SHOOT_ON;
                Shoot_Ctrl->friction_mode = FRICTION_ON;

                int16_t wheel_value = remote_get_dt7_channel(7); // 获取wheel值
                if (wheel_value == 0)
                {
                    Shoot_Ctrl->load_mode = LOAD_STOP;
                }
                else if (wheel_value > 0)
                {
                    Shoot_Ctrl->load_mode = LOAD_REVERSE;
                }
                else if (wheel_value < 0)
                {
                    Shoot_Ctrl->load_mode = LOAD_BURSTFIRE;
                }
            }

            // 底盘控制部分
            uint8_t sw2_state = remote_get_dt7_channel(6);
            if (sw2_state == DT7_SW_UP)
            {
                Chassis_Ctrl->chassis_mode = CHASSIS_FOLLOW_GIMBAL_YAW;
            }
            else if (sw2_state == DT7_SW_MID)
            {
                Chassis_Ctrl->chassis_mode = CHASSIS_ROTATE;
            }
            else if (sw2_state == DT7_SW_DOWN)
            {
                Chassis_Ctrl->chassis_mode = CHASSIS_ROTATE_REVERSE;
            }
        }
        else
        {
            // 遥控器离线处理
            Gimbal_Ctrl->gimbal_mode   = GIMBAL_ZERO_FORCE;
            Chassis_Ctrl->chassis_mode = CHASSIS_ZERO_FORCE;
            Shoot_Ctrl->shoot_mode     = SHOOT_OFF;
            Shoot_Ctrl->friction_mode  = FRICTION_OFF;
            Shoot_Ctrl->load_mode      = LOAD_STOP;
            memset(Chassis_Ctrl, 0, sizeof(Chassis_Ctrl_Cmd_s));
        }
    }
#endif
#endif

#if defined(REMOTE_VT_SOURCE)
// 图传遥控器处理
#if REMOTE_VT_SOURCE == 2 // VT03图传遥控器
    {
        static uint8_t   trigger_state      = 0; // 0: 关闭状态, 1: 开启状态
        static uint8_t   chassis_mode_state = 0; // 0: FOLLOW_GIMBAL_YAW, 1: ROTATE, 2: ROTATE_REVERSE
        VT03_Instance_t *vt03_instance      = NULL;
        vt03_instance                       = remote_get_vt03();
        if (Module_Offline_get_device_status(vt03_instance->offline_dev) == STATE_ONLINE && vt03_instance != NULL)
        {
            // 摇杆控制底盘移动
            Chassis_Ctrl->vx = -1.0f * remote_get_vt03_channel(2);
            Chassis_Ctrl->vy = 1.0f * remote_get_vt03_channel(1);

            // 云台控制部分
            uint8_t switch_pos = remote_get_vt03_channel(6);
            ;
            if (switch_pos == 0)
            {
                Gimbal_Ctrl->gimbal_mode = GIMBAL_GYRO_MODE;
                Gimbal_Ctrl->yaw -= 0.001f * (float)(remote_get_vt03_channel(4));
                Gimbal_Ctrl->pitch += 0.001f * (float)(remote_get_vt03_channel(3));
                VAL_LIMIT(Gimbal_Ctrl->pitch, SMALL_YAW_PITCH_MIN_ANGLE, SMALL_YAW_PITCH_MAX_ANGLE);
            }
            else if (switch_pos == 1)
            {
                Gimbal_Ctrl->gimbal_mode = GIMBAL_AUTO_MODE;
            }
            else if (switch_pos == 2)
            {
                Gimbal_Ctrl->gimbal_mode = GIMBAL_ZERO_FORCE;
            }

            // 发射机构控制部分
            // 只有当按钮从松开(0)变为按下(1)时才切换状态
            static uint8_t last_trigger_state = 0; // 存储上一次的trigger状态
            uint8_t        current_trigger    = remote_get_vt03_channel(10);
            if (current_trigger == 1 && last_trigger_state == 0)
            {
                trigger_state = !trigger_state; // 切换状态

                if (trigger_state)
                { // 开启状态
                    Shoot_Ctrl->shoot_mode    = SHOOT_ON;
                    Shoot_Ctrl->friction_mode = FRICTION_ON;

                    int16_t wheel_value = remote_get_vt03_channel(5);
                    if (wheel_value == 0)
                    {
                        Shoot_Ctrl->load_mode = LOAD_STOP;
                    }
                    else if (wheel_value > 0)
                    {
                        Shoot_Ctrl->load_mode = LOAD_REVERSE;
                    }
                    else if (wheel_value < 0)
                    {
                        Shoot_Ctrl->load_mode = LOAD_BURSTFIRE;
                    }
                }
                else
                { // 关闭状态
                    Shoot_Ctrl->shoot_mode    = SHOOT_ON;
                    Shoot_Ctrl->friction_mode = FRICTION_OFF;
                    Shoot_Ctrl->load_mode     = LOAD_STOP;
                }
            }

            // 底盘控制部分
            static uint8_t last_custom_right_state = 0; // 存储上一次的custom_right状态
            uint8_t        current_custom_right    = remote_get_vt03_channel(9);
            if (current_custom_right == 1 && last_custom_right_state == 0)
            {
                chassis_mode_state = (chassis_mode_state + 1) % 3; // 循环切换三种模式

                switch (chassis_mode_state)
                {
                case 0:
                    Chassis_Ctrl->chassis_mode = CHASSIS_FOLLOW_GIMBAL_YAW;
                    break;
                case 1:
                    Chassis_Ctrl->chassis_mode = CHASSIS_ROTATE;
                    break;
                case 2:
                    Chassis_Ctrl->chassis_mode = CHASSIS_ROTATE_REVERSE;
                    break;
                }
            }

            // 更新上一次的按键状态
            last_trigger_state      = current_trigger;
            last_custom_right_state = current_custom_right;
        }
        else
        {
            // 图传遥控器离线处理
            Gimbal_Ctrl->gimbal_mode   = GIMBAL_ZERO_FORCE;
            Chassis_Ctrl->chassis_mode = CHASSIS_ZERO_FORCE;
            Shoot_Ctrl->shoot_mode     = SHOOT_OFF;
            Shoot_Ctrl->friction_mode  = FRICTION_OFF;
            Shoot_Ctrl->load_mode      = LOAD_STOP;
            memset(Chassis_Ctrl, 0, sizeof(Chassis_Ctrl_Cmd_s));
        }
    }
#endif
#endif
}

void GimbalAuto(Chassis_Ctrl_Cmd_s *Chassis_Ctrl, Gimbal_Ctrl_Cmd_s *Gimbal_Ctrl, Ins_t *ins, Shoot_Ctrl_Cmd_s *Shoot_Ctrl,
                ReceivePacket *receive_packet)
{
    if (Gimbal_Ctrl == NULL || Gimbal_Ctrl->gimbal_mode != GIMBAL_AUTO_MODE)
    {
        return;
    }

    // 检查 USB 离线状态
    if (cdc_acm_usb_offline_state() == STATE_OFFLINE)
    {
        // 保持云台静止
        Gimbal_Ctrl->auto_search = 0;
        Gimbal_Ctrl->pitch       = SMALL_YAW_PITCH_HORIZON_ANGLE;
        Gimbal_Ctrl->yaw         = ins->YawTotalAngle_rad;
        // 保持发射机构停止
        Shoot_Ctrl->load_mode = LOAD_STOP;

        return;
    }
    // 底盘设置为自动模式
    Chassis_Ctrl->chassis_mode = CHASSIS_AUTOMODE;
    // USB 在线时，检查是否有有效数据包
    if (receive_packet != NULL && receive_packet->found != 0)
    {
        if (SMALL_YAW_PITCH_MIN_ANGLE < -receive_packet->target_pitch && -receive_packet->target_pitch < SMALL_YAW_PITCH_MAX_ANGLE)
        {
            Gimbal_Ctrl->auto_search = 0;
            Gimbal_Ctrl->pitch       = -receive_packet->target_pitch;
            Gimbal_Ctrl->yaw         = receive_packet->target_yaw + ins->YawRoundCount * 360.0f * DEGREE_2_RAD;
            if (receive_packet->fire_advice == 1)
            {
                Shoot_Ctrl->load_mode = LOAD_BURSTFIRE;
            }
            else
            {
                Shoot_Ctrl->load_mode = LOAD_STOP;
            }
        }
        else
        {
            Gimbal_Ctrl->auto_search = 1;
            Shoot_Ctrl->load_mode    = LOAD_STOP;
        }
    }
    else
    {
        // 无有效数据时进入搜索模式
        Gimbal_Ctrl->auto_search = 1;
        Shoot_Ctrl->load_mode    = LOAD_STOP;
    }

    if (receive_packet->nav_state == 1)
    {
        Chassis_Ctrl->vx = receive_packet->vx;
        Chassis_Ctrl->vy = receive_packet->vy;
    }
    else
    {
        Chassis_Ctrl->vx = 0;
        Chassis_Ctrl->vy = 0;
    }
}

void RefereetoGimbal(Chassis_Upload_Data_s *chassis_upload_data)
{
    const robot_status_t   *robot_status                 = (robot_status_t *)Module_Referee_Get_cmd_data(CMD_ID_ROBOT_STATUS);
    const robot_hp_data_t  *robot_hp                     = (robot_hp_data_t *)Module_Referee_Get_cmd_data(CMD_ID_ROBOT_HP);
    const allowed_bullet_t *allowed_bullet               = (allowed_bullet_t *)Module_Referee_Get_cmd_data(CMD_ID_ALLOWED_BULLET);
    const game_status_t    *game_status                  = (game_status_t *)Module_Referee_Get_cmd_data(CMD_ID_GAME_STATUS);
    chassis_upload_data->Robot_Color                     = (robot_status->robot_id <= 7) ? 0 : (robot_status->robot_id >= 100) ? 1 : 0; // 红方id为0-7
    chassis_upload_data->projectile_allowance_17mm       = (allowed_bullet->bullet_17mm_allowed < 0) ? 0 : allowed_bullet->bullet_17mm_allowed;
    chassis_upload_data->power_management_shooter_output = robot_status->power_output.shooter_output;
    chassis_upload_data->current_hp                      = robot_status->current_hp;
    chassis_upload_data->outpost_HP                      = robot_hp->ally_outpost_hp;
    chassis_upload_data->base_HP                         = robot_hp->ally_base_hp;
    chassis_upload_data->game_progess                    = game_status->type_progress.game_progress;
}

void RefereeUIset(Referee_Send_Ui_s *referee_ui, Shoot_Ctrl_Cmd_s *shoot_cmd, Ins_t *ins, ReceivePacket *receive_packet)
{
    referee_ui->frict = shoot_cmd->friction_mode;
    referee_ui->yaw   = ins->euler_rad[2];
    referee_ui->pitch = ins->euler_rad[1];
    if (!cdc_acm_usb_offline_state())
    {
        referee_ui->auto_aim = 1;
    }
    else
    {
        referee_ui->auto_aim = 0;
    }
    if (receive_packet->found)
    {
        referee_ui->auto_aim = 4;
    }
}

void SuperCap_Handle(Module_SuperCap_t *supercap)
{
    const robot_status_t    *robot_status    = (robot_status_t *)Module_Referee_Get_cmd_data(CMD_ID_ROBOT_STATUS);
    const power_heat_data_t *power_heat_data = (power_heat_data_t *)Module_Referee_Get_cmd_data(CMD_ID_POWER_HEAT_DATA);

    // 超电数据发送
    SuperCap_Send_t supercap_send;
    supercap_send.enableDCDC               = 1;
    supercap_send.systemRestart            = 0;
    supercap_send.feedbackJudgePowerLimit  = robot_status->chassis_power_limit;
    supercap_send.feedbackJudgePowerBuffer = power_heat_data->buffer_energy;
    Module_SuperCap_Send(&supercap_send);

    if (Module_Offline_get_device_status(supercap->offline_dev) != STATE_OFFLINE && supercap->receive_data.errorCode == 0)
    {
        // 超电接收处理
        if (supercap->receive_data.errorCode == 0 && supercap->receive_data.capEnergy > 200)
        {
            PowerControl_Set(robot_status->chassis_power_limit + 100, 60, 0);
        }
        else if (supercap->receive_data.errorCode == 0 && supercap->receive_data.capEnergy < 60)
        {
            PowerControl_Set(robot_status->chassis_power_limit - 20, 60, 0);
        }
        else
        {
            PowerControl_Set(robot_status->chassis_power_limit + 50, 60, 0);
        }
    }
    else
    {
        PowerControl_Set(robot_status->chassis_power_limit, 60, 0);
    }
}