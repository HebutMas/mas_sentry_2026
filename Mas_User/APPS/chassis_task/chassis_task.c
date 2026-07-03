#include "app_config.h"
#include "chassis_type.h"
#include "chassis_task.h"
#include "module_itc.h"
#include "module_offline.h"
#include "motor_def.h"
#include "motor_dji.h"
#include "referee_protocol.h"
#include "power_control.h"
#include "robot_def.h"
#include "bsp_dwt.h"
#include "tx_api.h"
#include "user_lib.h"
#include <stdint.h>

#define LOG_TAG "app_chassis"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                            chassis_thread;
CHASSIS_THREAD_STACK_SECTION static uint8_t chassis_thread_stack[CHASSIS_THREAD_STACK_SIZE];

// chassis 控制命令
static Chassis_Ctrl_Cmd_s *chassis_cmd; // 底盘接收到的控制命令
static itc_subscriber_t    chassis_sub;
static DJI_Motor_t        *chassis_motors[8];
static float               chassis_vx, chassis_vy, chassis_wz; // 将云台系的速度投影到底盘
static PIDInstance         chassis_follow_pid;
static Hit_Check_t         hit_check;
void                       ChassisInit(void)
{
    PID_Init_Config_s config = {
        .MaxOut = 10, .IntegralLimit = 0.01, .DeadBand = 1, .Kp = 0.3, .Ki = 0, .Kd = 0.001, .Improve = 0x01}; // enable integratiaon limit
    PIDInit(&chassis_follow_pid, &config);

    Motor_Init_Config_s chassis_motor_config = {
        .offline_init_conifig =
            {
                .timeout_ms = 100,            // 超时时间
                .enable     = OFFLINE_ENABLE, // 是否启用离线管理
            },
        .can_device_init_config =
            {
                .hcan = &hcan2,
            },
        .controller_init_config = {.lqr_init =
                                       {
                                           .K          = {0.008f},
                                           .output_max = 6.0f,
                                           .output_min = -6.0f,
                                           .state_dim  = 1,
                                       }},
        .setting_init_config =
            {
                .angle_feedback_source = 0,
                .speed_feedback_source = 0,
                .loop_type             = SPEED_LOOP,
                .feedback_reverse_flag = 0,
                .algorithm_type        = CONTROL_LQR,
            },
        .Motor_init_Info = {.motor_type = M3508, .gear_ratio = 16, .max_torque = 5.32, .torque_constant = 0.016},
    };

    PowerControl_Param_t power_config = {.k1 = 0.132, .k2 = 3.47, .k3 = 1};

    chassis_motor_config.can_device_init_config.tx_id           = 2;
    chassis_motor_config.setting_init_config.motor_reverse_flag = 0;
    chassis_motor_config.offline_init_conifig.name              = "m3508_2";
    chassis_motor_config.offline_init_conifig.beep_times        = 1;
    chassis_motors[0]                                           = Motor_DJI_Init(&chassis_motor_config);
    PowerControl_Motor_Init(chassis_motors[0], PC_ROLE_DRIVE, power_config);

    chassis_motor_config.can_device_init_config.tx_id           = 3;
    chassis_motor_config.setting_init_config.motor_reverse_flag = 0;
    chassis_motor_config.offline_init_conifig.name              = "m3508_3";
    chassis_motor_config.offline_init_conifig.beep_times        = 3;
    chassis_motors[1]                                           = Motor_DJI_Init(&chassis_motor_config);
    PowerControl_Motor_Init(chassis_motors[1], PC_ROLE_DRIVE, power_config);

    chassis_motor_config.can_device_init_config.tx_id           = 4;
    chassis_motor_config.setting_init_config.motor_reverse_flag = 1;
    chassis_motor_config.offline_init_conifig.name              = "m3508_4";
    chassis_motor_config.offline_init_conifig.beep_times        = 4;
    chassis_motors[2]                                           = Motor_DJI_Init(&chassis_motor_config);
    PowerControl_Motor_Init(chassis_motors[2], PC_ROLE_DRIVE, power_config);

    chassis_motor_config.can_device_init_config.tx_id           = 1;
    chassis_motor_config.setting_init_config.motor_reverse_flag = 1;
    chassis_motor_config.offline_init_conifig.name              = "m3508_1";
    chassis_motor_config.offline_init_conifig.beep_times        = 1;
    chassis_motors[3]                                           = Motor_DJI_Init(&chassis_motor_config);
    PowerControl_Motor_Init(chassis_motors[3], PC_ROLE_DRIVE, power_config);

    Motor_Init_Config_s gm6020_motor_config = {
        .offline_init_conifig =
            {
                .timeout_ms = 100,            // 超时时间
                .enable     = OFFLINE_ENABLE, // 是否启用离线管理
            },
        .can_device_init_config =
            {
                .hcan = &hcan1,
            },
        .controller_init_config = {.lqr_init =
                                       {
                                           .K          = {2.23f, 0.2f},
                                           .output_max = 2.23f,
                                           .output_min = -2.23f,
                                           .state_dim  = 2,
                                       }},
        .setting_init_config =
            {
                .angle_feedback_source = 0,
                .speed_feedback_source = 0,
                .loop_type             = ANGLE_LOOP,
                .feedback_reverse_flag = 0,
                .algorithm_type        = CONTROL_LQR,
            },
        .Motor_init_Info = {.motor_type = GM6020_CURRENT, .gear_ratio = 1, .max_torque = 2.23, .torque_constant = 0.741},
    };

    PowerControl_Param_t gm6020_power_config = {.k1 = 0.005, .k2 = 12.98, .k3 = 1};

    gm6020_motor_config.can_device_init_config.tx_id           = 2;
    gm6020_motor_config.setting_init_config.motor_reverse_flag = 0;
    gm6020_motor_config.offline_init_conifig.name              = "gm6020_2";
    gm6020_motor_config.offline_init_conifig.beep_times        = 5;
    chassis_motors[4]                                          = Motor_DJI_Init(&gm6020_motor_config);
    PowerControl_Motor_Init(chassis_motors[4], PC_ROLE_STEER, gm6020_power_config);

    gm6020_motor_config.can_device_init_config.tx_id           = 3;
    gm6020_motor_config.setting_init_config.motor_reverse_flag = 0;
    gm6020_motor_config.offline_init_conifig.name              = "gm6020_3";
    gm6020_motor_config.offline_init_conifig.beep_times        = 6;
    chassis_motors[5]                                          = Motor_DJI_Init(&gm6020_motor_config);
    PowerControl_Motor_Init(chassis_motors[5], PC_ROLE_STEER, gm6020_power_config);

    gm6020_motor_config.can_device_init_config.tx_id           = 4;
    gm6020_motor_config.setting_init_config.motor_reverse_flag = 0;
    gm6020_motor_config.offline_init_conifig.name              = "gm6020_4";
    gm6020_motor_config.offline_init_conifig.beep_times        = 7;
    chassis_motors[6]                                          = Motor_DJI_Init(&gm6020_motor_config);
    PowerControl_Motor_Init(chassis_motors[6], PC_ROLE_STEER, gm6020_power_config);

    gm6020_motor_config.can_device_init_config.tx_id           = 1;
    gm6020_motor_config.setting_init_config.motor_reverse_flag = 0;
    gm6020_motor_config.offline_init_conifig.name              = "gm6020_1";
    gm6020_motor_config.offline_init_conifig.beep_times        = 8;
    chassis_motors[7]                                          = Motor_DJI_Init(&gm6020_motor_config);
    PowerControl_Motor_Init(chassis_motors[7], PC_ROLE_STEER, gm6020_power_config);

    UINT status;
    status = Module_Itc_subscriber_init(&chassis_sub, "chassis_cmd_topic", sizeof(Chassis_Ctrl_Cmd_s));
    if (status != TX_SUCCESS)
    {
        log_e("chassis subscriber init failed");
        return;
    }
}

void chassis_thread_entry(ULONG thread_input)
{
    for (;;)
    {
        const game_status_t *game_status = (game_status_t *)Module_Referee_Get_cmd_data(CMD_ID_GAME_STATUS);
        chassis_cmd                      = (Chassis_Ctrl_Cmd_s *)Module_Itc_receive(&chassis_sub);
        if (chassis_cmd != NULL)
        {
            if (!Module_Offline_get_device_status(chassis_motors[0]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[1]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[2]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[3]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[4]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[5]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[6]->offline_dev) &&
                !Module_Offline_get_device_status(chassis_motors[7]->offline_dev))
            {
                if (chassis_cmd->chassis_mode == CHASSIS_ZERO_FORCE)
                {
                    Motor_DJI_Stop(chassis_motors[0]);
                    Motor_DJI_Stop(chassis_motors[1]);
                    Motor_DJI_Stop(chassis_motors[2]);
                    Motor_DJI_Stop(chassis_motors[3]);
                    Motor_DJI_Stop(chassis_motors[4]);
                    Motor_DJI_Stop(chassis_motors[5]);
                    Motor_DJI_Stop(chassis_motors[6]);
                    Motor_DJI_Stop(chassis_motors[7]);
                }
                else
                {
                    Motor_DJI_Start(chassis_motors[0]);
                    Motor_DJI_Start(chassis_motors[1]);
                    Motor_DJI_Start(chassis_motors[2]);
                    Motor_DJI_Start(chassis_motors[3]);
                    Motor_DJI_Start(chassis_motors[4]);
                    Motor_DJI_Start(chassis_motors[5]);
                    Motor_DJI_Start(chassis_motors[6]);
                    Motor_DJI_Start(chassis_motors[7]);
                }

                // 根据控制模式设定旋转速度
                switch (chassis_cmd->chassis_mode)
                {
                case CHASSIS_ROTATE_REVERSE: // 自旋反转,同时保持全向机动
                    chassis_wz = -8;
                    break;
                case CHASSIS_FOLLOW_GIMBAL_YAW: // 跟随云台
                    PIDCalculate(&chassis_follow_pid, chassis_cmd->offset_angle * RAD_2_DEGREE, 0);
                    chassis_wz = chassis_follow_pid.Output;
                    break;
                case CHASSIS_ROTATE: // 自旋,同时保持全向机动
                    chassis_wz = 3;
                    break;
                case CHASSIS_AUTOMODE:
                    if (game_status->type_progress.game_progress == 4) // 比赛中
                    {
                        if (chassis_cmd->vx == 0 && chassis_cmd->vy == 0 && CheckRobotBeingHit(&hit_check) == 0)
                        {
                            chassis_wz = 3;
                        }
                        // else if (chassis_cmd->vx != 0 && chassis_cmd->vy != 0 && CheckRobotBeingHit(&hit_check) == 0)
                        // {
                        //     PIDCalculate(&chassis_follow_pid, chassis_cmd->offset_angle * RAD_2_DEGREE, 0);
                        //     chassis_wz = chassis_follow_pid.Output;
                        // }
                        if (chassis_cmd->vx == 0 && chassis_cmd->vy == 0 && CheckRobotBeingHit(&hit_check) == 1)
                        {
                            chassis_wz = 10;
                        }
                        else if (chassis_cmd->vx != 0 && chassis_cmd->vy != 0 && CheckRobotBeingHit(&hit_check) == 1)
                        {
                            chassis_wz = 2;
                        }
                    }
                    else
                    {
                        chassis_vx = 0;
                        chassis_vy = 0;
                        chassis_wz = 0;
                    }
                    break;
                default:
                    break;
                }

                // 根据云台和底盘的角度offset将控制量映射到底盘坐标系上
                // 底盘逆时针旋转为角度正方向;云台命令的方向以云台指向的方向为x,采用右手系
                // CHASSIS_FORWARD_OFFSET_RAD: 底盘正方向偏置角，用于在不修改ALIGN_RAD的情况下对齐新的正方向
                static float sin_theta, cos_theta;
                float        total_angle = chassis_cmd->offset_angle + CHASSIS_FORWARD_OFFSET_RAD;
                cos_theta                = arm_cos_f32(total_angle);
                sin_theta                = arm_sin_f32(total_angle);
                chassis_vx               = chassis_cmd->vx * cos_theta - chassis_cmd->vy * sin_theta;
                chassis_vy               = chassis_cmd->vx * sin_theta + chassis_cmd->vy * cos_theta;

                Chassis_Calculate_Target(chassis_motors, chassis_vy, chassis_vx, chassis_wz);
            }
            else
            {
                Motor_DJI_Stop(chassis_motors[0]);
                Motor_DJI_Stop(chassis_motors[1]);
                Motor_DJI_Stop(chassis_motors[2]);
                Motor_DJI_Stop(chassis_motors[3]);
                Motor_DJI_Stop(chassis_motors[4]);
                Motor_DJI_Stop(chassis_motors[5]);
                Motor_DJI_Stop(chassis_motors[6]);
                Motor_DJI_Stop(chassis_motors[7]);
            }
        }

        tx_thread_sleep(2);
    }
}

uint8_t CheckRobotBeingHit(Hit_Check_t *hit_check)
{
    const robot_status_t *robot_status = (robot_status_t *)Module_Referee_Get_cmd_data(CMD_ID_ROBOT_STATUS);

    if (robot_status == NULL)
    {
        return 0;
    }
    if (hit_check->is_first_check)
    {
        hit_check->last_hp        = robot_status->current_hp;
        hit_check->is_first_check = 0;
    }

    uint32_t current_time = DWT_GetTimeline_ms();
    // 检测血量是否减少
    if (robot_status->current_hp < hit_check->last_hp)
    {
        hit_check->is_being_hit  = 1;
        hit_check->last_hit_time = current_time;
    }
    // 检查是否超过5秒没有受到新的伤害
    else if (hit_check->is_being_hit && (current_time - hit_check->last_hit_time > 5000))
    {
        hit_check->is_being_hit = 0;
    }

    // 更新上一次的血量值
    hit_check->last_hp = robot_status->current_hp;

    return hit_check->is_being_hit;
}

void chassis_task_init()
{
    ChassisInit();

    UINT status = tx_thread_create(&chassis_thread, "chassisTask", chassis_thread_entry, 0, chassis_thread_stack, CHASSIS_THREAD_STACK_SIZE,
                                   CHASSIS_THREAD_PRIORITY, CHASSIS_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create chassis task!");
        return;
    }
    log_i("chassis task created");
}
