#include "ins_task.h"
#include "app_config.h"
#include "module_ins.h"
#include "tx_api.h"
#include <stdint.h>

#define LOG_TAG "app_ins"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                        ins_thread;
INS_THREAD_STACK_SECTION static uint8_t ins_thread_stack[INS_THREAD_STACK_SIZE];

void ins_task(ULONG thread_input)
{
    for (;;)
    {
        Module_Ins_task_func();
        tx_thread_sleep(1);
    }
}

void ins_task_init()
{
    Module_Ins_init();

    UINT status = tx_thread_create(&ins_thread, "insTask", ins_task, 0, ins_thread_stack,
                                   INS_THREAD_STACK_SIZE, INS_THREAD_PRIORITY, INS_THREAD_PRIORITY,
                                   TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create INS task!");
        return;
    }

    log_i("INS task started successfully.");
}