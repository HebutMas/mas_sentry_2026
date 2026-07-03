#include "app_config.h"
#include "module_supercap.h"
#include "tx_api.h"
#include <stdint.h>

#define LOG_TAG "app_supercap"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                               supercap_thread;
SUPERCAP_THREAD_STACK_SECTION static uint8_t supercap_thread_stack[SUPERCAP_THREAD_STACK_SIZE];

void supercap_task(ULONG thread_input)
{
    while (1)
    {
        Module_SuperCap_func();
    }
}

void supercap_task_init(void)
{
    UINT status;

    status = Module_SuperCap_Init();
    if (status != TX_SUCCESS)
    {
        log_e("Failed to initialize supercap module!");
        return;
    }

    status = tx_thread_create(&supercap_thread, "supercap_Task", supercap_task, 0, supercap_thread_stack, SUPERCAP_THREAD_STACK_SIZE,
                              SUPERCAP_THREAD_PRIORITY, SUPERCAP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create supercap task!");
        return;
    }

    log_i("supercap task started successfully.");
}
