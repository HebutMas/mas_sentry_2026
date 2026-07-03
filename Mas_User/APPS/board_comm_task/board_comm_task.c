#include "board_comm_task.h"
#include "app_config.h"
#include "module_board_comm.h"
#include "tx_api.h"
#include <stdint.h>

#define LOG_TAG "app_board_comm"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                               board_comm_thread;
BOARD_COMM_THREAD_STACK_SECTION static uint8_t board_comm_thread_stack[BOARD_COMM_THREAD_STACK_SIZE];

void board_comm_task(ULONG thread_input)
{
    while (1)
    {
        Module_Board_comm_func();
    }
}

void board_comm_task_init(void)
{
    UINT status;

    status = Module_Board_comm_init();
    if (status != TX_SUCCESS)
    {
        log_e("Failed to initialize board_comm module!");
        return;
    }

    status = tx_thread_create(&board_comm_thread, "board_comm_Task", board_comm_task, 0, board_comm_thread_stack, BOARD_COMM_THREAD_STACK_SIZE,
                              BOARD_COMM_THREAD_PRIORITY, BOARD_COMM_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create board_comm task!");
        return;
    }

    log_i("board_comm task started successfully.");
}
