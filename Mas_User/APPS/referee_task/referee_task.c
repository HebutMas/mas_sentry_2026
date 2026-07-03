/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-03-12 19:27:18
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-13 18:19:01
 * @FilePath: \mas_rm_djic\Mas_User\APPS\referee_task\referee_task.c
 * @Description:
 */
#include "referee_task.h"
#include "app_config.h"
#include "module_itc.h"
#include "referee_protocol.h"
#include "tx_api.h"
#include "referee_user_ui.h"
#include <stdint.h>

#define LOG_TAG "app_referee"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static TX_THREAD                              referee_rx_thread;
REFREE_THREAD_RX_STACK_SECTION static uint8_t referee_rx_thread_stack[REFREE_THREAD_RX_STACK_SIZE];
static TX_THREAD                              referee_tx_thread;
REFREE_THREAD_TX_STACK_SECTION static uint8_t referee_tx_thread_stack[REFREE_THREAD_TX_STACK_SIZE];

// referee_ui接收
static itc_subscriber_t   referee_sub;
static Referee_Send_Ui_s *referee_cmd = NULL;

void referee_tx_task(ULONG thread_input)
{
    referee_ui_init();
    while (1)
    {
        referee_cmd = (Referee_Send_Ui_s *)Module_Itc_receive(&referee_sub);
        if (referee_cmd != NULL)
        {
            referee_ui_update_by_user(referee_cmd->chassis, referee_cmd->frict, (int32_t)(referee_cmd->pitch * 1000.0f),
                                      (int32_t)(referee_cmd->yaw * 1000.0f), referee_cmd->offset_angle * 57.2958f, referee_cmd->super_cap_pct,
                                      referee_cmd->auto_aim);
        }
        referee_ui_update_by_referee();
        tx_thread_sleep(100);
    }
}

void referee_rx_task(ULONG thread_input)
{
    while (1)
    {
        Module_Referee_Decode_func();
    }
}

void referee_task_init()
{
    Module_Referee_Init();

    UINT status;
    // referee_ui_sub初始化
    status = Module_Itc_subscriber_init(&referee_sub, "referee_ui_topic", sizeof(Referee_Send_Ui_s));
    if (status != TX_SUCCESS)
    {
        log_e("referee_ui subscriber init failed");
        return;
    }

    status = tx_thread_create(&referee_rx_thread, "referee_rx_Task", referee_rx_task, 0, referee_rx_thread_stack, REFREE_THREAD_RX_STACK_SIZE,
                              REFREE_THREAD_RX_PRIORITY, REFREE_THREAD_RX_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    status = tx_thread_create(&referee_tx_thread, "referee_tx_Task", referee_tx_task, 0, referee_tx_thread_stack, REFREE_THREAD_TX_STACK_SIZE,
                              REFREE_THREAD_TX_PRIORITY, REFREE_THREAD_TX_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create referee task!");
        return;
    }

    log_i("referee task started successfully.");
}
