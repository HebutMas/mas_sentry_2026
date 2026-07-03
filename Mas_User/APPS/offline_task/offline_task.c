/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-19 13:02:45
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-26 16:58:37
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/APPS/offline_task/offline_task.c
 * @Description:
 */
#include "offline_task.h"
#include "app_config.h"
#include "iwdg.h"
#include "module_offline.h"
#include "tx_api.h"
#include <stdint.h>

#define LOG_TAG "app_offline"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                            offline_thread;
OFFLINE_THREAD_STACK_SECTION static uint8_t offline_thread_stack[OFFLINE_THREAD_STACK_SIZE];
static TX_TIMER                             offline_timer;

void offline_task(ULONG thread_input)
{
#if OFFLINE_WATCHDOG_ENABLE
#if defined(STM32H723xx)
    MX_IWDG1_Init();
    __HAL_DBGMCU_FREEZE_IWDG1();
#elif defined(STM32F407xx)
    __HAL_DBGMCU_FREEZE_IWDG();
    MX_IWDG_Init();
#endif
#endif
    for (;;)
    {
#if OFFLINE_WATCHDOG_ENABLE
#if defined(STM32H723xx)
        HAL_IWDG_Refresh(&hiwdg1);
#elif defined(STM32F407xx)
        HAL_IWDG_Refresh(&hiwdg);
#endif
#endif
        Module_Offline_task_function();
        tx_thread_sleep(10);
    }
}

void offline_task_init()
{
    Module_Offline_init();

    UINT status = tx_timer_create(&offline_timer, "offline_timer", Module_offline_beep_ctrl_times,
                                  0, 10, 10, TX_AUTO_ACTIVATE);

    status = tx_thread_create(&offline_thread, "offlineTask", offline_task, 0, offline_thread_stack,
                              OFFLINE_THREAD_STACK_SIZE, OFFLINE_THREAD_PRIORITY,
                              OFFLINE_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create offline task!");
        return;
    }

    log_i("Offline task started successfully.");
}
