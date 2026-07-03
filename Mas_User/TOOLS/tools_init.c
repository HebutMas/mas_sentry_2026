/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 10:13:56
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-13 14:53:38
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/TOOLS/tools_init.c
 * @Description:
 */
#include "tools_init.h"
#include "SEGGER_RTT.h"
#include "tool_log.h"

void tools_init(void) {
    SEGGER_RTT_Init();
    log_init();
    log_i("Tools initialized successfully.");
}
