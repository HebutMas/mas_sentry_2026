#include "robot_control_task.h"
#include "app_config.h"
#include "module_board_comm.h"
#include "module_ins.h"
#include "module_itc.h"
#include "module_supercap.h"
#include "power_control.h"
#include "robot_config.h"
#include "robot_control_fun.h"
#include "robot_def.h"
#include "tx_port.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "app_robot_control"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                                  robot_control_thread;
ROBOT_CONTROL_THREAD_STACK_SECTION static uint8_t robot_control_thread_stack[ROBOT_CONTROL_THREAD_STACK_SIZE];

#if defined(ONE_BOARD)
// 云台发射机构底盘命令发布
static itc_topic_t        gimbal_cmd_topic;
static Gimbal_Ctrl_Cmd_s  gimbal_cmd;
static itc_topic_t        shoot_cmd_topic;
static Shoot_Ctrl_Cmd_s   shoot_cmd;
static itc_topic_t        chassis_cmd_topic;
static Chassis_Ctrl_Cmd_s chassis_cmd;
#elif defined(GIMBAL_BOARD)
// 虚拟串口数据结构体
static ReceivePacket *receive_packet = NULL;
static SendPacket     send_packet;
// 姿态角数据
static Ins_t *ins = NULL;
// 云台与发射机构命令发布
static itc_topic_t       gimbal_cmd_topic;
static Gimbal_Ctrl_Cmd_s gimbal_cmd;
static itc_topic_t       shoot_cmd_topic;
static Shoot_Ctrl_Cmd_s  shoot_cmd;
// 云台反馈数据
static Gimbal_Upload_Data_s *gimbal_upload_data;
static itc_subscriber_t      gimbal_upload_sub;
// 板间通讯部分
static Chassis_Ctrl_Cmd_s     chassis_cmd;
static Referee_Send_Ui_s      referee_ui;
static Chassis_Upload_Data_s *chassis_data;
#elif defined(CHASSIS_BOARD)
// chassis命令
static itc_topic_t chassis_cmd_topic;
// referee ui topic
static itc_topic_t referee_ui_topic;
// 板间通讯部分
static Chassis_Ctrl_Cmd_s   *chassis_cmd = NULL;
static Referee_Send_Ui_s    *referee_ui  = NULL;
static Chassis_Upload_Data_s chassis_data;
static Module_SuperCap_t    *supercap;
#else
#error "Please define board type in robot_config.h!"
#endif

void robot_control_init(void)
{
#if defined(ONE_BOARD)

#elif defined(GIMBAL_BOARD)
    // ins 初始化
    ins = Module_Ins_get();
    if (ins == NULL)
    {
        log_e("ins is null");
        return;
    }
    UINT status;
    // gimbal shoot topic 初始化
    status = Module_Itc_topic_init(&gimbal_cmd_topic, "gimbal_cmd_topic", sizeof(Gimbal_Ctrl_Cmd_s));
    if (status != TX_SUCCESS)
    {
        log_e("gimbal topic init failed!");
        return;
    }
    status = Module_Itc_topic_init(&shoot_cmd_topic, "shoot_cmd_topic", sizeof(Shoot_Ctrl_Cmd_s));
    if (status != TX_SUCCESS)
    {
        log_e("shoot topic init failed!");
        return;
    }
    status = Module_Itc_subscriber_init(&gimbal_upload_sub, "gimbal_upload_topic", sizeof(Gimbal_Upload_Data_s));
    if (status != TX_SUCCESS)
    {
        log_e("gimbal upload subscriber init failed!");
        return;
    }
#elif defined(CHASSIS_BOARD)
    UINT status;
    // chassis cmd topic 初始化
    status = Module_Itc_topic_init(&chassis_cmd_topic, "chassis_cmd_topic", sizeof(Chassis_Ctrl_Cmd_s));
    if (status != TX_SUCCESS)
    {
        log_e("chassis topic init failed!");
        return;
    }
    // referee ui topic 初始化
    status = Module_Itc_topic_init(&referee_ui_topic, "referee_ui_topic", sizeof(Referee_Send_Ui_s));
    if (status != TX_SUCCESS)
    {
        log_e("referee ui topic init failed!");
        return;
    }
#else
#error "Please define board type in robot_config.h!"
#endif
}

void robot_control_task(ULONG thread_input)
{
    while (1)
    {
#if defined(ONE_BOARD)

#elif defined(GIMBAL_BOARD)
        // 获取遥控器控制命令
        RemoteControlSet(&chassis_cmd, &shoot_cmd, &gimbal_cmd);
        // 虚拟串口通信
        send_packet.header                          = 0xAA;
        send_packet.mode                            = 0;
        send_packet.q[0]                            = ins->q[0];
        send_packet.q[1]                            = ins->q[1];
        send_packet.q[2]                            = ins->q[2];
        send_packet.q[3]                            = ins->q[3];
        send_packet.current_hp                      = chassis_data->current_hp;
        send_packet.power_management_shooter_output = chassis_data->power_management_shooter_output;
        send_packet.outpost_HP                      = chassis_data->outpost_HP;
        send_packet.projectile_allowance_17mm       = chassis_data->projectile_allowance_17mm;
        send_packet.game_progess                    = chassis_data->game_progess;
        send_packet.base_HP                         = chassis_data->base_HP;
        send_packet.tail                            = 0x5A;
        cdc_acm_data_send(&send_packet, TX_NO_WAIT);
        receive_packet = cdc_acm_read_data();
        // 自动模式下云台控制
        if (gimbal_cmd.gimbal_mode == GIMBAL_AUTO_MODE)
        {
            GimbalAuto(&chassis_cmd, &gimbal_cmd, ins, &shoot_cmd, receive_packet);
        }
        // gimbal shoot命令发布
        Module_Itc_publish(&gimbal_cmd_topic, &gimbal_cmd);
        Module_Itc_publish(&shoot_cmd_topic, &shoot_cmd);
        // 云台反馈数据获取
        gimbal_upload_data = (Gimbal_Upload_Data_s *)Module_Itc_receive(&gimbal_upload_sub);
        if (gimbal_upload_data != NULL)
        {
            chassis_cmd.offset_angle = CalcOffsetAngle(gimbal_upload_data->yaw_ecd);
        }
        else
        {
            chassis_cmd.offset_angle = 0;
        }
        // 板间通讯
        RefereeUIset(&referee_ui, &shoot_cmd, ins, receive_packet);
        Module_Board_comm_send(&chassis_cmd, &referee_ui);
        // 接收chassis上传数据
        chassis_data = Module_Board_comm_get_chassis_upload();
#elif defined(CHASSIS_BOARD)
        // 裁判系统信息上传
        RefereetoGimbal(&chassis_data);
        // 板间通讯处理
        Module_Board_comm_send(&chassis_data);
        // 接收chassis命令
        chassis_cmd = Module_Board_comm_get_chassis_ctrl();
        if (chassis_cmd != NULL)
        {
            // chassis cmd topic 发布
            Module_Itc_publish(&chassis_cmd_topic, chassis_cmd);
        }

        // 接收referee ui数据
        referee_ui = Module_Board_comm_get_referee_ui();
        // 超电部分处理
        supercap = Module_SuperCap_Get();
        if (supercap != NULL)
        {
            SuperCap_Handle(supercap);
            referee_ui->super_cap_pct = supercap->receive_data.capEnergy;
        }
        else
        {
            referee_ui->super_cap_pct = 0;
        }
        if (referee_ui != NULL)
        {
            Module_Itc_publish(&referee_ui_topic, referee_ui);
        }
#else
#error "Please define board type in robot_config.h!"
#endif
        tx_thread_sleep(2);
    }
}

void robot_control_task_init(void)
{
    UINT status;

    robot_control_init();

    status = tx_thread_create(&robot_control_thread, "robot_control_thread", robot_control_task, 0, robot_control_thread_stack,
                              ROBOT_CONTROL_THREAD_STACK_SIZE, ROBOT_CONTROL_THREAD_PRIORITY, ROBOT_CONTROL_THREAD_PRIORITY, TX_NO_TIME_SLICE,
                              TX_AUTO_START);
    if (status != TX_SUCCESS)
    {
        log_e("robot_control_task_init failed!");
        return;
    }

    log_i("robot_control_task_init success!");
}
