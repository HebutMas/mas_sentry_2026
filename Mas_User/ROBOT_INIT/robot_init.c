/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 10:11:10
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-28 02:41:02
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/ROBOT_INIT/robot_init.c
 * @Description: 
 */
#include "robot_init.h"
#include "app_init.h"
#include "bsp_init.h"
#include "tools_init.h"

#define LOG_TAG "robot_init"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

void robot_init(void) {
    tools_init(); 
    BSP_Init();
    app_init();
    log_i("robot init success");
}