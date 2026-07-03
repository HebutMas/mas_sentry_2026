#include "module_board_comm.h"
#include "bsp_can.h"
#include "module_offline.h"
#include "robot_config.h"
#include "robot_def.h"
#include "user_lib.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "module_board_comm"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static Module_Board_Comm_t board_comm;

UINT Module_Board_comm_init()
{
#ifdef ONE_BOARD
    return TX_DELETED;
#elif defined(GIMBAL_BOARD)
    Can_Device_Init_Config_s board_com_init = {
        .hcan  = BAORD_COMM_CAN,
        .rx_id = CHASSIS_ID,
        .tx_id = GIMBAL_ID,
    };
    board_comm.candevice = BSP_CAN_Device_Init(&board_com_init);
    if (board_comm.candevice == NULL)
    {
        log_e("can device init failed");
        return TX_DELETED;
    }
    Offline_Init_config_t offline_manage_init = {.name = "board_comm", .timeout_ms = 500, .beep_times = 9, .enable = OFFLINE_ENABLE};
    board_comm.offline_dev                    = Module_Offline_device_register(&offline_manage_init);
    if (board_comm.offline_dev == NULL)
    {
        log_e("offline device init failed");
        return TX_DELETED;
    }
#elif defined(CHASSIS_BOARD)
    Can_Device_Init_Config_s board_com_init = {
        .hcan  = BAORD_COMM_CAN,
        .rx_id = GIMBAL_ID,
        .tx_id = CHASSIS_ID,
    };
    board_comm.candevice = BSP_CAN_Device_Init(&board_com_init);
    if (board_comm.candevice == NULL)
    {
        log_e("can device init failed");
        return TX_DELETED;
    }
    Offline_Init_config_t offline_manage_init = {.name = "board_comm", .timeout_ms = 500, .beep_times = 9, .enable = OFFLINE_ENABLE};
    board_comm.offline_dev                    = Module_Offline_device_register(&offline_manage_init);
    if (board_comm.offline_dev == NULL)
    {
        log_e("offline device init failed");
        return TX_DELETED;
    }
#endif
    board_comm.initialized = 1;
    log_i("board_comm init sucess");
    return TX_SUCCESS;
}

void Module_Board_comm_func()
{
    if (board_comm.initialized != 1)
    {
        tx_thread_sleep(100);
    }

#ifdef GIMBAL_BOARD
    // 读取CAN数据
    if (BSP_CAN_ReadSingleDevice(board_comm.candevice, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        // 拷贝数据
        board_comm.chassis_upload_data.Robot_Color                     = (board_comm.candevice->rx_buf[0] >> 7) & 0x01;
        uint8_t compressed_projectile                                  = board_comm.candevice->rx_buf[0] & 0x7F;
        board_comm.chassis_upload_data.projectile_allowance_17mm       = (compressed_projectile * 1000) / 127;
        board_comm.chassis_upload_data.power_management_shooter_output = (board_comm.candevice->rx_buf[1] >> 7) & 0x01;
        board_comm.chassis_upload_data.current_hp                      = ((board_comm.candevice->rx_buf[1] & 0x7F) * 400) / 100;

        uint16_t outpost                            = (board_comm.candevice->rx_buf[2] << 3) | ((board_comm.candevice->rx_buf[3] >> 5) & 0x07);
        board_comm.chassis_upload_data.outpost_HP   = (outpost * 1500) / 2047;
        uint16_t base                               = ((board_comm.candevice->rx_buf[3] & 0x1F) << 8) | board_comm.candevice->rx_buf[4];
        board_comm.chassis_upload_data.base_HP      = (base * 5000) / 8191;
        board_comm.chassis_upload_data.game_progess = board_comm.candevice->rx_buf[5];

        Module_Offline_device_update(board_comm.offline_dev);
    }
#elif defined(CHASSIS_BOARD)
    // 读取CAN数据
    if (BSP_CAN_ReadSingleDevice(board_comm.candevice, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        uint8_t *data = board_comm.candevice->rx_buf;

        // 解析数据
        board_comm.chassis_ctrl_cmd.vx = (float)((int8_t)data[0]) / 10.0f;
        board_comm.chassis_ctrl_cmd.vy = (float)((int8_t)data[1]) / 10.0f;
        board_comm.chassis_ctrl_cmd.wz = (float)((int8_t)data[2]) / 10.0f;

        // offset_angle ×1000，int16_t
        int16_t angle_int                        = (int16_t)(((uint16_t)data[3] << 8) | data[4]);
        board_comm.chassis_ctrl_cmd.offset_angle = (float)angle_int / 1000.0f;
        board_comm.referee_send_ui.offset_angle  = (float)angle_int / 1000.0f;

        // buf[5]位域解析
        board_comm.chassis_ctrl_cmd.chassis_mode = (chassis_mode_e)(data[5] & 0x07u);
        board_comm.referee_send_ui.chassis       = (chassis_mode_e)(data[5] & 0x07u);
        board_comm.referee_send_ui.frict         = (friction_mode_e)((data[5] >> 3u) & 0x01u);
        board_comm.referee_send_ui.auto_aim      = (int8_t)((data[5] >> 4u) & 0x07u);

        // pitch 角度制（°）
        board_comm.referee_send_ui.pitch = (float)((int8_t)data[6]);
        // yaw 角度制（°）
        board_comm.referee_send_ui.yaw = (float)((int8_t)data[7]);

        Module_Offline_device_update(board_comm.offline_dev);
    }
#else
    tx_thread_sleep(100);
#endif
}

#ifdef GIMBAL_BOARD

void Module_Board_comm_send(const Chassis_Ctrl_Cmd_s *data, const Referee_Send_Ui_s *referee_ui)
{
    // 参数检查
    if (data == NULL || board_comm.initialized != 1)
    {
        return;
    }

    uint8_t buf[8] = {0};

    float vx_scaled = data->vx * 10.0f;
    VAL_LIMIT(vx_scaled, -127, 127);
    buf[0] = (uint8_t)(int8_t)vx_scaled;

    float vy_scaled = data->vy * 10.0f;
    VAL_LIMIT(vy_scaled, -127, 127);
    buf[1] = (uint8_t)(int8_t)vy_scaled;

    float wz_scaled = data->wz * 10.0f;
    VAL_LIMIT(wz_scaled, -127, 127);
    buf[2] = (uint8_t)(int8_t)wz_scaled;

    int16_t angle_int = (int16_t)(data->offset_angle * 1000.0f);
    buf[3]            = (uint8_t)((angle_int >> 8) & 0xFF);
    buf[4]            = (uint8_t)(angle_int & 0xFF);

    // buf[5]: bit0~2=chassis_mode  bit3=frict  bit4~5=auto_aim
    buf[5] = (uint8_t)((uint8_t)data->chassis_mode & 0x07u);
    if (referee_ui != NULL)
    {
        buf[5] |= (uint8_t)(((uint8_t)referee_ui->frict & 0x01u) << 3u);
        buf[5] |= (uint8_t)(((uint8_t)referee_ui->auto_aim & 0x07u) << 4u);
    }

    float pitch_deg = (referee_ui != NULL) ? referee_ui->pitch * 57.2958f : 0.0f;
    VAL_LIMIT(pitch_deg, -127, 127);
    buf[6] = (uint8_t)(int8_t)pitch_deg;

    float yaw_deg = (referee_ui != NULL) ? referee_ui->yaw * 57.2958f : 0.0f;
    VAL_LIMIT(yaw_deg, -127, 127);
    buf[7] = (uint8_t)(int8_t)yaw_deg;

    // 发送数据
    BSP_CAN_SendDevice(board_comm.candevice, buf, 8);
}

Chassis_Upload_Data_s *Module_Board_comm_get_chassis_upload(void)
{
    // 参数检查
    if (board_comm.initialized != 1)
    {
        return NULL;
    }
    return &board_comm.chassis_upload_data;
}
#endif

#ifdef CHASSIS_BOARD

Chassis_Ctrl_Cmd_s *Module_Board_comm_get_chassis_ctrl(void)
{
    // 参数检查
    if (board_comm.initialized != 1)
    {
        return NULL;
    }
    return &board_comm.chassis_ctrl_cmd;
}

Referee_Send_Ui_s *Module_Board_comm_get_referee_ui(void)
{
    // 参数检查
    if (board_comm.initialized != 1)
    {
        return NULL;
    }
    return &board_comm.referee_send_ui;
}

void Module_Board_comm_send(Chassis_Upload_Data_s *data)
{
    // 参数检查
    if (data == NULL || board_comm.initialized != 1)
    {
        return;
    } // 拷贝数据到发送缓冲区
    uint8_t buf[8];

    int16_t projectile = data->projectile_allowance_17mm;
    if (projectile < 0) projectile = 0;
    if (projectile > 1000) projectile = 1000;
    uint8_t compressed_projectile = (projectile * 127) / 1000;

    // 第1字节: Robot_Color(1位) + projectile_allowance_17mm前7位
    buf[0] = (data->Robot_Color & 0x01) << 7;
    buf[0] |= (compressed_projectile & 0x7F);
    // 第2字节: power_management_shooter_output(1位) + current_hp_percent(7位)
    buf[1] = (data->power_management_shooter_output & 0x01) << 7;
    buf[1] |= (data->current_hp * 100 / 400) & 0x7F;
    // 第3-4字节: outpost_HP(11位)
    uint16_t outpost = (data->outpost_HP * 2047 / 1500) & 0x07FF;
    buf[2]           = outpost >> 3;
    buf[3]           = (outpost & 0x07) << 5;
    // 第4-5字节: base_HP(13位)
    uint16_t base = (data->base_HP * 8191 / 5000) & 0x1FFF;
    buf[3] |= (base >> 8) & 0x1F;
    buf[4] = base & 0xFF;
    // 第6字节: game_progess(3位) + game_time高6位
    buf[5] = data->game_progess;
    // 发送数据
    BSP_CAN_SendDevice(board_comm.candevice, buf, 8);
}
#endif