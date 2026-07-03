/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-27 23:07:01
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-27 23:43:50
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/APPS/wt606_task/wt606_task.c
 * @Description:
 */
#include "wt606_task.h"
#include "app_config.h"
#include "module_wt606.h"
#include "tx_api.h"
#include <stdint.h>

#define LOG_TAG "app_ins"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                        wt_thread;
WT_THREAD_STACK_SECTION static uint8_t wt_thread_stack[WT_THREAD_STACK_SIZE];

void wt_task(ULONG thread_input)
{
    for (;;)
    {
        Module_WT606_task_function();
    }
}

void wt606_task_init()
{
    Module_WT606_init();

    UINT status = tx_thread_create(&wt_thread, "wtTask", wt_task, 0, wt_thread_stack,
                                   WT_THREAD_STACK_SIZE, WT_THREAD_PRIORITY, WT_THREAD_PRIORITY,
                                   TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create WT task!");
        return;
    }

    log_i("WT task started successfully.");
}