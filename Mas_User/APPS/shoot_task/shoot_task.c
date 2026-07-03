/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-28 23:45:14
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-26 19:30:21
 * @FilePath: \mas_rm_djic\Mas_User\APPS\shoot_task\shoot_task.c
 * @Description:
 */
#include "shoot_task.h"
#include "app_config.h"
#include "module_itc.h"
#include "module_offline.h"
#include "motor_def.h"
#include "motor_dji.h"
#include "robot_def.h"
#include "tx_api.h"
#include "user_lib.h"
#include <string.h>

#define LOG_TAG "app_shoot"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                          shootTask_thread;
SHOOT_THREAD_STACK_SECTION static uint8_t shoot_thread_stack[SHOOT_THREAD_STACK_SIZE];

DJI_Motor_t             *friction_l = NULL;
DJI_Motor_t             *friction_r = NULL;
DJI_Motor_t             *loader     = NULL; // 拨盘电机
static Shoot_Ctrl_Cmd_s *shoot_cmd;         // 来自cmd的发射控制信息
static itc_subscriber_t  shoot_cmd_sub;

void shoot_init(void)
{
    Motor_Init_Config_s friction_config = {
        .offline_init_conifig =
            {
                .name       = "3508_1",       // 设备名称
                .timeout_ms = 100,            // 超时时间
                .beep_times = 5,              // 蜂鸣次数
                .enable     = OFFLINE_ENABLE, // 是否启用离线管理
            },
        .can_device_init_config =
            {
                .hcan = &hcan1,
            },
        .controller_init_config = {.lqr_init =
                                       {
                                           .K          = {0.0011f},
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
        .Motor_init_Info = {.motor_type = M3508, .gear_ratio = 19, .max_torque = 6, .torque_constant = 0.0016f},
    };
    // 左摩擦轮
    friction_config.can_device_init_config.tx_id = 1;
    friction_l                                   = Motor_DJI_Init(&friction_config);
    if (friction_l == NULL)
    {
        log_e("friction_l init failed");
        return;
    }
    // 右摩擦轮
    friction_config.can_device_init_config.tx_id    = 2; // 右摩擦轮,改txid和方向就行
    friction_config.offline_init_conifig.name       = "3508_2";
    friction_config.offline_init_conifig.beep_times = 6;
    friction_r                                      = Motor_DJI_Init(&friction_config);
    if (friction_r == NULL)
    {
        log_e("friction_r init failed");
        return;
    }

    // 拨盘电机
    Motor_Init_Config_s loader_config = {
        .offline_init_conifig =
            {
                .name       = "m2006",        // 设备名称
                .timeout_ms = 100,            // 超时时间
                .beep_times = 7,              // 蜂鸣次数
                .enable     = OFFLINE_ENABLE, // 是否启用离线管理
            },
        .can_device_init_config =
            {
                .hcan  = &hcan1,
                .tx_id = 3,
            },
        .controller_init_config = {.lqr_init =
                                       {
                                           .K          = {0.0034f}, // 0.0317
                                           .output_max = 1.8f,
                                           .output_min = -1.8f,
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
        .Motor_init_Info =
            {
                .motor_type      = M2006,
                .gear_ratio      = 36,
                .max_torque      = 1.8,
                .torque_constant = 0.005f,
            },
    };
    loader = Motor_DJI_Init(&loader_config);
    if (loader == NULL)
    {
        log_e("loader init failed");
        return;
    }

    UINT status;
    status = Module_Itc_subscriber_init(&shoot_cmd_sub, "shoot_cmd_topic", sizeof(Shoot_Ctrl_Cmd_s));
    if (status != TX_SUCCESS)
    {
        log_e("shoot subscriber init failed");
        return;
    }
}

void shoot_task(ULONG thread_input)
{
    while (1)
    {
        shoot_cmd = (Shoot_Ctrl_Cmd_s *)Module_Itc_receive(&shoot_cmd_sub);
        if (shoot_cmd != NULL)
        {
            // 从cmd获取控制数据
            if (!Module_Offline_get_device_status(friction_l->offline_dev) && !Module_Offline_get_device_status(friction_r->offline_dev) &&
                !Module_Offline_get_device_status(loader->offline_dev))
            {
                if (shoot_cmd->shoot_mode == SHOOT_ON)
                {
                    Motor_DJI_Start(friction_l);
                    Motor_DJI_Start(friction_r);
                    Motor_DJI_Start(loader);
                    // 确定是否开启摩擦轮
                    if (shoot_cmd->friction_mode == FRICTION_ON)
                    {
                        // 根据收到的弹速设置设定摩擦轮电机参考值,需实测后填入
                        Motor_DJI_SetRef(friction_l, 7300 * RPM_2_RAD_PER_SEC);
                        Motor_DJI_SetRef(friction_r, -7300 * RPM_2_RAD_PER_SEC);
                        switch (shoot_cmd->load_mode)
                        {
                        // 停止拨盘
                        case LOAD_STOP:
                            Motor_DJI_SetRef(loader, 0);
                            break;
                        case LOAD_1_BULLET:
                            Motor_DJI_SetRef(loader, -4000 * RPM_2_RAD_PER_SEC);
                            break;
                        case LOAD_BURSTFIRE:
                            Motor_DJI_SetRef(loader, -8000 * RPM_2_RAD_PER_SEC);
                            break;
                        default:
                            break;
                        }
                    }
                    else // 关闭摩擦轮
                    {
                        Motor_DJI_SetRef(friction_l, 0);
                        Motor_DJI_SetRef(friction_r, 0);
                        Motor_DJI_SetRef(loader, 0);
                    }
                }
                else
                {
                    Motor_DJI_Stop(friction_l);
                    Motor_DJI_Stop(friction_r);
                    Motor_DJI_Stop(loader);
                }
            }
            else
            {
                Motor_DJI_Stop(friction_l);
                Motor_DJI_Stop(friction_r);
                Motor_DJI_Stop(loader);
            }
        }
        tx_thread_sleep(2);
    }
}

void shoot_task_init()
{
    shoot_init();

    UINT status = tx_thread_create(&shootTask_thread, "shootTask", shoot_task, 0, shoot_thread_stack, SHOOT_THREAD_STACK_SIZE, SHOOT_THREAD_PRIORITY,
                                   SHOOT_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create shoot task!");
        return;
    }
    log_i("shoot task init sucess");
}
