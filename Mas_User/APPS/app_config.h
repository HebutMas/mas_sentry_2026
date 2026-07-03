/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-14 12:51:31
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-28 23:58:43
 * @FilePath: \mas_rm_djic\Mas_User\APPS\app_config.h
 * @Description:
 */
#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include "bsp_def.h"

/* 线程配置文件 */

/* offline 线程配置 */
#define OFFLINE_THREAD_STACK_SIZE          1024               // 离线检测线程栈大小
#define OFFLINE_THREAD_STACK_SECTION       APPS_STACK_SECTION // 线程栈内存区域
#define OFFLINE_THREAD_PRIORITY            1                  // 离线检测线程优先级

/* ins 线程配置 */
#define INS_THREAD_STACK_SIZE              1024               // ins检测线程栈大小
#define INS_THREAD_STACK_SECTION           APPS_STACK_SECTION // 线程栈内存区域
#define INS_THREAD_PRIORITY                2                  // ins线程优先级

/* remote 线程配置 */
#define REMOTE_THREAD_STACK_SIZE           1024               // remote检测线程栈大小
#define REMOTE_THREAD_STACK_SECTION        APPS_STACK_SECTION // 线程栈内存区域
#define REMOTE_THREAD_PRIORITY             3                  // remote线程优先级
#define REMOTE_VT_THREAD_STACK_SIZE        1024               // remote检测线程栈大小
#define REMOTE_VT_THREAD_STACK_SECTION     APPS_STACK_SECTION // 线程栈内存区域
#define REMOTE_VT_THREAD_PRIORITY          4                  // remote线程优先级

/* motor_task 线程配置 */
#define MOTOR_CONTROL_THREAD_STACK_SIZE    1024               // 电机控制线程栈大小
#define MOTOR_CONTROL_THREAD_STACK_SECTION APPS_STACK_SECTION // 线程栈内存区域
#define MOTOR_CONTROL_THREAD_PRIORITY      6                  // 电机控制线程优先级
#define MOTOR_DECODE_THREAD_STACK_SIZE     1024               // 电机解码线程栈大小
#define MOTOR_DECODE_THREAD_STACK_SECTION  APPS_STACK_SECTION // 线程栈内存区域
#define MOTOR_DECODE_THREAD_PRIORITY       5                  // 电机解码线程优先级

/* wt606 线程配置 */
#define WT_THREAD_STACK_SIZE               1024               // wt606线程栈大小
#define WT_THREAD_STACK_SECTION            APPS_STACK_SECTION // 线程栈内存区域
#define WT_THREAD_PRIORITY                 7                  // wt606线程优先级

/* robot_control 线程配置 */
#define ROBOT_CONTROL_THREAD_STACK_SIZE    1024               // robot_control线程栈大小
#define ROBOT_CONTROL_THREAD_STACK_SECTION APPS_STACK_SECTION // 线程栈内存区域
#define ROBOT_CONTROL_THREAD_PRIORITY      10                 // robot_control线程优先级

/* gimbal 线程配置 */
#define GIMBAL_THREAD_STACK_SIZE           1024               // gimbal线程栈大小
#define GIMBAL_THREAD_STACK_SECTION        APPS_STACK_SECTION // 线程栈内存区域
#define GIMBAL_THREAD_PRIORITY             11                 // gimbal线程优先级

/* shoot 线程配置 */
#define SHOOT_THREAD_STACK_SIZE            1024               // shoot线程栈大小
#define SHOOT_THREAD_STACK_SECTION         APPS_STACK_SECTION // 线程栈内存区域
#define SHOOT_THREAD_PRIORITY              12                 // shoot线程优先级

/* chassis 线程配置 */
#define CHASSIS_THREAD_STACK_SIZE          1024               // chassis线程栈大小
#define CHASSIS_THREAD_STACK_SECTION       APPS_STACK_SECTION // 线程栈内存区域
#define CHASSIS_THREAD_PRIORITY            11                 // chassis线程优先级

/* referee 线程配置 */
#define REFREE_THREAD_RX_STACK_SIZE        1024               // referee接收线程栈大小
#define REFREE_THREAD_RX_STACK_SECTION     APPS_STACK_SECTION // 线程栈内存区域
#define REFREE_THREAD_RX_PRIORITY          3                  // referee接收线程优先级
#define REFREE_THREAD_TX_STACK_SIZE        1024               // referee发送线程栈大小
#define REFREE_THREAD_TX_STACK_SECTION     APPS_STACK_SECTION // 线程栈内存区域
#define REFREE_THREAD_TX_PRIORITY          20                 // referee发送线程优先级

/* board_comm 线程配置 */
#define BOARD_COMM_THREAD_STACK_SIZE       1024               // board_comm线程栈大小
#define BOARD_COMM_THREAD_STACK_SECTION    APPS_STACK_SECTION // 线程栈内存区域
#define BOARD_COMM_THREAD_PRIORITY         8                 // board_comm线程优先级

/* supercap 线程配置 */
#define SUPERCAP_THREAD_STACK_SIZE         1024               // supercap线程栈大小
#define SUPERCAP_THREAD_STACK_SECTION      APPS_STACK_SECTION // 线程栈内存区域
#define SUPERCAP_THREAD_PRIORITY           9                  // supercap线程优先级

#endif // _APP_CONFIG_H_