/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-02-04 23:23:59
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-26 17:48:50
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\BOARD_COMM\module_board_comm.h
 * @Description:
 */
#ifndef _BOARD_COMM_H_
#define _BOARD_COMM_H_

#include "bsp_can.h"
#include "module_offline.h"
#include "robot_config.h"
#include "robot_def.h"

#if defined(STM32H723xx)
#define BAORD_COMM_CAN &hfdcan2
#elif defined(STM32F407xx)
#define BAORD_COMM_CAN &hcan2
#endif

#define GIMBAL_ID  0X310 // 云台板ID
#define CHASSIS_ID 0X311 // 底盘板ID

// 板间通信结构体
typedef struct
{
    uint8_t        initialized;
    Can_Device    *candevice;
    OfflineDevice *offline_dev;
#if defined(GIMBAL_BOARD)
    Chassis_Upload_Data_s chassis_upload_data;
#elif defined(CHASSIS_BOARD)
    Chassis_Ctrl_Cmd_s chassis_ctrl_cmd;
    Referee_Send_Ui_s  referee_send_ui;
#endif
} Module_Board_Comm_t;

/**
 * @description: 初始化板间通信模块
 * @return {UINT}, TX_SUCCESS 成功,其余失败
 */
UINT Module_Board_comm_init();
/**
 * @description: 板间通信模块主函数，在线程中调用
*/
void Module_Board_comm_func();

/**
 * @brief 发送数据,云台板发 Cmd/ui数据，底盘板发 Upload Data
 * @param data 待发送的数据指针
 * @param timeout 超时时间
 */
#if defined(GIMBAL_BOARD)
void Module_Board_comm_send(const Chassis_Ctrl_Cmd_s *data, const Referee_Send_Ui_s *referee_ui);
#elif defined(CHASSIS_BOARD)
void Module_Board_comm_send(Chassis_Upload_Data_s *data);
#endif

/**
 * @brief 获取接收到的最新数据
 * @return 数据指针
 */
#if defined(GIMBAL_BOARD)
Chassis_Upload_Data_s *Module_Board_comm_get_chassis_upload(void);
#elif defined(CHASSIS_BOARD)
Chassis_Ctrl_Cmd_s *Module_Board_comm_get_chassis_ctrl(void);
Referee_Send_Ui_s  *Module_Board_comm_get_referee_ui(void);
#endif

#endif // _BOARD_COMM_H_