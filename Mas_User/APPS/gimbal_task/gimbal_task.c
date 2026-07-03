#include "gimbal_task.h"
#include "app_config.h"
#include "module_bmi088.h"
#include "module_ins.h"
#include "module_itc.h"
#include "module_offline.h"
#include "module_wt606.h"
#include "motor_damiao.h"
#include "motor_dji.h"
#include "robot_config.h"
#include "robot_def.h"
#include "user_lib.h"
#include <math.h>
#include <string.h>

// 自动搜索参数
#define SEARCH_YAW_SPEED_RPS   1.5f   // yaw 旋转速度 (rad/s)，任务周期 2ms
#define SEARCH_PITCH_AMPLITUDE 0.3f   // pitch 正弦幅值 (rad)
#define SEARCH_PITCH_FREQ      0.8f   // pitch 正弦频率 (Hz)
#define SEARCH_TASK_PERIOD_S   0.002f // 任务周期 (s)
// 自动搜索状态
static float search_yaw_angle  = 0.0f; // 累积 yaw 目标角度 (rad)
static float search_pitch_time = 0.0f; // pitch 正弦计时 (s)
static float auto_yaw_offset   = 0.0f; // 自动模式下的 yaw 偏移量（相对于 IMU 的差值）

#define LOG_TAG "app_ins"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

// gimbal 线程定义
static TX_THREAD                           gimbal_thread;
GIMBAL_THREAD_STACK_SECTION static uint8_t gimbal_thread_stack[GIMBAL_THREAD_STACK_SIZE];
// gimbal 电机指针定义
static DJI_Motor_t *big_yaw_motor   = NULL; // 大云台yaw电机指针
static DJI_Motor_t *small_yaw_motor = NULL; // 小云台yaw电机指针
static DM_MOTOR_t  *pitch_motor     = NULL; // pitch电机指针
// 姿态角数据
static Ins_t                 *ins          = NULL;
static Bmi088_device_t       *bmi088_dev   = NULL;
static Module_WT606_Device_t *wt606_device = NULL;
static float                  big_yaw_offset;
// gimbal命令接收
static itc_subscriber_t   gimbal_sub;
static Gimbal_Ctrl_Cmd_s *gimbal_cmd = NULL;
// gimbal数据反馈
static itc_topic_t          gimbal_upload_topic;
static Gimbal_Upload_Data_s gimbal_upload_data;

void gimbal_init(void)
{
    ins = Module_Ins_get();
    if (ins == NULL)
    {
        log_e("ins is null");
        return;
    }
    bmi088_dev = Module_BMI088_get_device();
    if (bmi088_dev == NULL)
    {
        log_e("bmi088_dev is null");
        return;
    }
    wt606_device = Module_WT606_get_device();
    if (wt606_device == NULL)
    {
        log_e("wt606_device is null");
        return;
    }
    Motor_Init_Config_s small_yaw_config = {
        .offline_init_conifig =
            {
                .name       = "6020_small",
                .timeout_ms = 100,
                .beep_times = 3,
                .enable     = OFFLINE_ENABLE,
            },
        .can_device_init_config =
            {
                .hcan  = &hcan1,
                .tx_id = 1,
            },
        .controller_init_config = {.other_angle_feedback_ptr = &ins->YawTotalAngle_rad,
                                   .other_speed_feedback_ptr = &bmi088_dev->gyro[2],
                                   .lqr_init =
                                       {
                                           .K          = {5.47f, 0.56f},
                                           .output_max = 2.223,
                                           .output_min = -2.223,
                                           .state_dim  = 2,
                                       }},
        .setting_init_config =
            {
                .algorithm_type        = CONTROL_LQR,
                .feedback_reverse_flag = 0,
                .angle_feedback_source = 1,
                .speed_feedback_source = 1,
                .loop_type             = ANGLE_LOOP,
            },
        .Motor_init_Info = {.motor_type = GM6020_CURRENT, .gear_ratio = 1, .max_torque = 2.223, .torque_constant = 0.741f}};
    small_yaw_motor = Motor_DJI_Init(&small_yaw_config);
    if (small_yaw_motor == NULL)
    {
        log_e("small_yaw_motor init failed");
        return;
    }

    Motor_Init_Config_s yaw_config = {
        .offline_init_conifig =
            {
                .name       = "6020_big",
                .timeout_ms = 100,
                .beep_times = 2,
                .enable     = OFFLINE_ENABLE,
            },
        .can_device_init_config =
            {
                .hcan  = &hcan2,
                .tx_id = 1,
            },
        .controller_init_config = {.other_angle_feedback_ptr = &wt606_device->YawTotalAngle_rad,
                                   .other_speed_feedback_ptr = &wt606_device->gyro_rps[2],
                                   .lqr_init =
                                       {
                                           .K          = {22.36f, 3.15f},
                                           .output_max = 2.223,
                                           .output_min = -2.223,
                                           .state_dim  = 2,
                                       }},
        .setting_init_config =
            {
                .algorithm_type        = CONTROL_LQR,
                .feedback_reverse_flag = 0,
                .angle_feedback_source = 1,
                .speed_feedback_source = 1,
                .loop_type             = ANGLE_LOOP,
            },
        .Motor_init_Info = {.motor_type = GM6020_CURRENT, .gear_ratio = 1, .max_torque = 2.223, .torque_constant = 0.741f}};
    big_yaw_motor = Motor_DJI_Init(&yaw_config);
    if (big_yaw_motor == NULL)
    {
        log_e("big_yaw_motor init failed");
        return;
    }

    // PITCH
    Motor_Init_Config_s pitch_config = {.offline_init_conifig =
                                            {
                                                .name       = "dm4310",
                                                .timeout_ms = 100,
                                                .beep_times = 4,
                                                .enable     = OFFLINE_ENABLE,
                                            },
                                        .can_device_init_config =
                                            {
                                                .hcan  = &hcan1,
                                                .tx_id = 0X23,
                                                .rx_id = 0X206,
                                            },
                                        .controller_init_config =
                                            {
                                                .other_angle_feedback_ptr = &ins->euler_rad[1],
                                                .other_speed_feedback_ptr = &bmi088_dev->gyro[0], // c板的pitch轴角速度，根据实际选择对应角速度
                                                .lqr_init =
                                                    {
                                                        .K           = {38.72983346f, 2.84576645f}, // 28.7312f,2.5974f
                                                        .output_max  = 10,
                                                        .output_min  = -10,
                                                        .state_dim   = 2,
                                                        .comp_params = 2.68f,
                                                        .feedback_comp_rad = &ins->euler_rad[1],
                                                    },
                                            },
                                        .setting_init_config =
                                            {
                                                .algorithm_type        = CONTROL_LQR,
                                                .feedback_reverse_flag = 1,
                                                .angle_feedback_source = 1,
                                                .speed_feedback_source = 1,
                                                .loop_type             = ANGLE_LOOP,
                                            },
                                        .Motor_init_Info = {.motor_type = DM4310, .gear_ratio = 10, .max_torque = 10, .torque_constant = 0.093f}};
    pitch_motor                      = Motor_DM_Init(&pitch_config, DM_MIT_MODE);
    if (pitch_motor == NULL)
    {
        log_e("pitch_motor init failed");
        return;
    }

    UINT status;
    // gimbal 命令初始化
    status = Module_Itc_subscriber_init(&gimbal_sub, "gimbal_cmd_topic", sizeof(Gimbal_Ctrl_Cmd_s));
    if (status != TX_SUCCESS)
    {
        log_e("gimbal subscriber init failed");
        return;
    }
    // gimbal 数据反馈初始化
    status = Module_Itc_topic_init(&gimbal_upload_topic, "gimbal_upload_topic", sizeof(Gimbal_Upload_Data_s));
    if (status != TX_SUCCESS)
    {
        log_e("gimbal upload topic init failed");
        return;
    }

    log_i("gimbal init sucess");
}

void gimbal_task(ULONG thread_input)
{
    while (1)
    {
        gimbal_cmd = (Gimbal_Ctrl_Cmd_s *)Module_Itc_receive(&gimbal_sub);
        if (gimbal_cmd != NULL)
        {
            if (!Module_Offline_get_device_status(big_yaw_motor->offline_dev) && !Module_Offline_get_device_status(small_yaw_motor->offline_dev) &&
                !Module_Offline_get_device_status(pitch_motor->offline_dev))
            {
                // 大yaw偏移量，用于实现跟随小yaw
                big_yaw_offset = wt606_device->YawTotalAngle_rad + (small_yaw_motor->measure.ecd - SMALL_YAW_ALIGN_ECD) * (2.0f * PI / 8192.0f);
                switch (gimbal_cmd->gimbal_mode)
                {
                case GIMBAL_ZERO_FORCE:
                    Motor_DJI_Stop(big_yaw_motor);
                    Motor_DJI_Stop(small_yaw_motor);
                    Motor_DM_Stop(pitch_motor);
                    break;
                case GIMBAL_GYRO_MODE:
                    Motor_DJI_Start(big_yaw_motor);
                    Motor_DJI_Start(small_yaw_motor);
                    Motor_DM_Start(pitch_motor);
                    // 大小 yaw 协调控制
                    Motor_DJI_SetRef(big_yaw_motor, big_yaw_offset);
                    Motor_DJI_SetRef(small_yaw_motor, gimbal_cmd->yaw * DEGREE_2_RAD + auto_yaw_offset);
                    Motor_DM_SetRef(pitch_motor, gimbal_cmd->pitch * DEGREE_2_RAD);
                    search_yaw_angle = ins->YawTotalAngle_rad;
                    // Motor_DM_SetRef(pitch_motor, ins->euler_rad[1]);
                    break;
                case GIMBAL_AUTO_MODE:
                    Motor_DJI_Start(big_yaw_motor);
                    Motor_DJI_Start(small_yaw_motor);
                    Motor_DM_Start(pitch_motor);
                    if (gimbal_cmd->auto_search == 0)
                    {
                        //   大小 yaw 协调控制 - 目标已找到
                        Motor_DJI_SetRef(big_yaw_motor, big_yaw_offset);
                        Motor_DJI_SetRef(small_yaw_motor, gimbal_cmd->yaw);
                        Motor_DM_SetRef(pitch_motor, gimbal_cmd->pitch);
                        // 目标找到，重置搜索状态，记录当前偏移量
                        search_yaw_angle  = gimbal_cmd->yaw;
                        search_pitch_time = 0.0f;
                        // 计算并保存偏移量
                        auto_yaw_offset = gimbal_cmd->yaw;
                    }
                    else if (gimbal_cmd->auto_search == 1)
                    {
                        gimbal_cmd->yaw   = 0;
                        gimbal_cmd->pitch = 0;
                        // 自动搜索：small yaw 持续旋转，pitch 正弦摆动
                        search_yaw_angle += SEARCH_YAW_SPEED_RPS * SEARCH_TASK_PERIOD_S;
                        search_pitch_time += SEARCH_TASK_PERIOD_S;
                        float search_pitch_ref = SEARCH_PITCH_AMPLITUDE * sinf(2.0f * PI * SEARCH_PITCH_FREQ * search_pitch_time);
                        search_pitch_ref       = fabsf(search_pitch_ref);
                        Motor_DJI_SetRef(big_yaw_motor, big_yaw_offset);
                        Motor_DJI_SetRef(small_yaw_motor, search_yaw_angle);
                        Motor_DM_SetRef(pitch_motor, -search_pitch_ref);
                        // 实时更新偏移量
                        auto_yaw_offset = ins->YawTotalAngle_rad;
                    }
                    break;
                }
            }
            else
            {
                Motor_DJI_Stop(big_yaw_motor);
                Motor_DJI_Stop(small_yaw_motor);
                Motor_DM_Stop(pitch_motor);
            }
        }
        // 数据反馈
        if (!Module_Offline_get_device_status(big_yaw_motor->offline_dev))
        {
            gimbal_upload_data.yaw_ecd = big_yaw_motor->measure.ecd;
            Module_Itc_publish(&gimbal_upload_topic, &gimbal_upload_data);
        }
        tx_thread_sleep(2);
    }
}

void gimbal_task_init()
{
    gimbal_init();

    UINT status;
    status = tx_thread_create(&gimbal_thread, "gimbal_task", gimbal_task, 0, gimbal_thread_stack, GIMBAL_THREAD_STACK_SIZE, GIMBAL_THREAD_PRIORITY,
                              GIMBAL_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS)
    {
        log_e("gimbal task create failed");
        return;
    }

    log_i("gimbal task init sucess");
}