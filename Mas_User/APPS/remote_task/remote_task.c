/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-24 01:45:18
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-24 10:21:02
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/APPS/remote_task/remote_task.c
 * @Description:
 */
#include "remote_task.h"
#include "app_config.h"
#include "dt7.h"
#include "remote_data.h"
#include "sbus.h"
#include "vt02.h"
#include "vt03.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "app_remote"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                              remote_thread;
REMOTE_THREAD_STACK_SECTION static uint8_t    remote_thread_stack[REMOTE_THREAD_STACK_SIZE];
static TX_THREAD                              remote_vt_thread;
REMOTE_VT_THREAD_STACK_SECTION static uint8_t remote_vt_thread_stack[REMOTE_VT_THREAD_STACK_SIZE];

void remote_task(ULONG thread_input)
{
    (void)thread_input;
    while (1)
    {
        // 根据遥控器类型读取数据
#if defined(REMOTE_SOURCE) && REMOTE_SOURCE == 1
        remote_sbus_decode_task_func();
#elif defined(REMOTE_SOURCE) && REMOTE_SOURCE == 2
        remote_dt7_decode_task_func();
#else
#error "No device was selected"
#endif
    }
}

void remote_vt_task(ULONG thread_input)
{
    (void)thread_input;
    while (1)
    {
        // 根据图传遥控器类型读取数据
#if defined(REMOTE_VT_SOURCE) && REMOTE_VT_SOURCE == 1
        remote_vt02_decode_task_func();
#elif defined(REMOTE_VT_SOURCE) && REMOTE_VT_SOURCE == 2
        remote_vt03_decode_task_func();
#else
        tx_thread_sleep(100);
#endif
    }
}

void remote_task_init()
{
#if defined(REMOTE_SOURCE) && REMOTE_SOURCE == 1
    remote_sbus_init();
#elif defined(REMOTE_SOURCE) && REMOTE_SOURCE == 2
    remote_dt7_init();
#else
#error "No device was selected"
#endif

#if defined(REMOTE_VT_SOURCE) && REMOTE_VT_SOURCE == 1
    remote_vt02_init();
#elif defined(REMOTE_VT_SOURCE) && REMOTE_VT_SOURCE == 2
    remote_vt03_init();
#endif
    UINT status = tx_thread_create(
        &remote_thread, "remoteTask", remote_task, 0, remote_thread_stack, REMOTE_THREAD_STACK_SIZE,
        REMOTE_THREAD_PRIORITY, REMOTE_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    status = tx_thread_create(&remote_vt_thread, "remoteVtTask", remote_vt_task, 0,
                              remote_vt_thread_stack, REMOTE_VT_THREAD_STACK_SIZE,
                              REMOTE_VT_THREAD_PRIORITY, REMOTE_VT_THREAD_PRIORITY,
                              TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create remote/vt task!");
        return;
    }

    log_i("remote/vt task started successfully.");
}