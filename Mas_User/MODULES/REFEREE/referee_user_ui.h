/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-03-12 19:53:25
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-13 17:26:58
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\REFEREE\referee_user_ui.h
 * @Description:
 */
#ifndef _REFEREE_USER_UI_H_
#define _REFEREE_USER_UI_H_

#include "referee_protocol.h"
#include "robot_def.h"
#include <stdint.h>

// 改为实际机器人 ID 和对应选手端 ID
#define UI_SELF_ROBOT_ID  ROBOT_ID_BLUE_INFANTRY_3
#define UI_SELF_CLIENT_ID CLIENT_ID_BLUE_INFANTRY_3

void referee_ui_init(void);

void referee_ui_update_by_user(chassis_mode_e chassis, friction_mode_e frict, int32_t pitch_x1000, int32_t yaw_x1000, float alginangle, uint16_t pct,
                               uint8_t auto_aim_online);

void referee_ui_update_by_referee(void);

#endif // _REFEREE_USER_UI_H_
