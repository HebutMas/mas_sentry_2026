/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-28 02:40:47
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-29 00:36:50
 * @FilePath: \mas_rm_djic\Mas_User\APPS\app_init.c
 * @Description:
 */
#include "app_init.h"
#include "board_comm_task.h"
#include "gimbal_task.h"
#include "ins_task.h"
#include "motor_task.h"
#include "offline_task.h"
#include "referee_task.h"
#include "remote_task.h"
#include "robot_control_task.h"
#include "shoot_task.h"
#include "wt606_task.h"
#include "chassis_task.h"
#include "robot_config.h"
#include "bsp_cdc_acm.h"
#include "supercap_task.h"

#define LOG_TAG "app_init"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

void app_init(void)
{
    offline_task_init();
    ins_task_init();
#if defined(GIMBAL_BOARD) || defined(ONE_BOARD)
    cdc_acm_offline_register();
    remote_task_init();
    wt606_task_init();
#endif
    board_comm_task_init();
    robot_control_task_init();
#if defined(GIMBAL_BOARD) || defined(ONE_BOARD)
    gimbal_task_init();
    shoot_task_init();
#elif defined(CHASSIS_BOARD)
    referee_task_init();
    supercap_task_init();
    chassis_task_init();
#endif
    motor_task_init();
    log_i("app init success");
}