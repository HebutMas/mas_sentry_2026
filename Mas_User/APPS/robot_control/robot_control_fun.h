/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-10-26 15:51:58
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-29 00:09:14
 * @FilePath: \mas_rm_djic\Mas_User\APPS\robot_control\robot_control_fun.h
 * @Description:
 */
#ifndef _ROBOT_CONTROL_FUN_H_
#define _ROBOT_CONTROL_FUN_H_

#include "bsp_cdc_acm.h"
#include "module_ins.h"
#include "module_supercap.h"
#include "robot_def.h"

float CalcOffsetAngle(float getyawangle);
void  RemoteControlSet(Chassis_Ctrl_Cmd_s *Chassis_Ctrl, Shoot_Ctrl_Cmd_s *Shoot_Ctrl, Gimbal_Ctrl_Cmd_s *Gimbal_Ctrl);

void GimbalAuto(Chassis_Ctrl_Cmd_s *Chassis_Ctrl, Gimbal_Ctrl_Cmd_s *Gimbal_Ctrl, Ins_t *ins, Shoot_Ctrl_Cmd_s *Shoot_Ctrl,
                ReceivePacket *receive_packet);
void RefereetoGimbal(Chassis_Upload_Data_s *chassis_upload_data);
void RefereeUIset(Referee_Send_Ui_s *referee_ui, Shoot_Ctrl_Cmd_s *shoot_cmd, Ins_t *ins, ReceivePacket *receive_packet);

void SuperCap_Handle(Module_SuperCap_t *supercap);

#endif // _ROBOT_CONTROL_FUN_H_