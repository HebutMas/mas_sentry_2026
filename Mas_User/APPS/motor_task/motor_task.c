/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-25 23:31:21
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-26 00:48:20
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/APPS/motor_task/motor_task.c
 * @Description:
 */
#include "motor_task.h"
#include "app_config.h"
#include "bsp_can.h"
#include "motor_damiao.h"
#include "motor_dji.h"
#include "motor_register.h"
#include <stdint.h>

#define LOG_TAG "app_motor"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

// clang-format off
static TX_THREAD motor_control_thread;
MOTOR_CONTROL_THREAD_STACK_SECTION static uint8_t motor_control_thread_stack[MOTOR_CONTROL_THREAD_STACK_SIZE];
static TX_THREAD motor_decode_thread;
MOTOR_DECODE_THREAD_STACK_SECTION static uint8_t motor_decode_thread_stack[MOTOR_DECODE_THREAD_STACK_SIZE];
// clang-format on

void motor_control_task(ULONG thread_input)
{
    while (1)
    {
        Motor_DJI_Control();
        Motor_DM_Control();
        tx_thread_sleep(2);
    }
}

void motor_decode_task(ULONG thread_input)
{
    uint32_t triggered_flag;
    while (1)
    {
        // 等待任一电机触发
        triggered_flag = BSP_CAN_ReadMultipleDevice(0xFFFFFFFF, TX_WAIT_FOREVER);

        if (triggered_flag == 0)
            tx_thread_sleep(10);

        // 因为 TX_OR_CLEAR 清空了标志位，这里必须把本次唤醒的所有标志位都处理完，
        while (triggered_flag != 0)
        {
            // 取出最低位
            uint32_t current_flag = triggered_flag & (-triggered_flag);

            // 查表 (O(1))
            void *motor_ptr = MotorRegistryLookup(current_flag);

            if (motor_ptr != NULL)
            {
                Motor_Type_e type = MotorRegistryGetType(current_flag);
                switch (type)
                {
                case GM6020_CURRENT:
                case GM6020_VOLTAGE:
                case M3508:
                case M2006:
                    Motor_DJI_Decode((DJI_Motor_t *)motor_ptr);
                    break;
                case DM4310:
                case DM6220:
                    Motor_DM_Decode((DM_MOTOR_t *)motor_ptr);
                    break;
                default:
                    break;
                }
            }
            // 移除已处理位
            triggered_flag ^= current_flag;
        }
    }
}

void motor_task_init(void)
{
    // 创建并启动电机控制任务
    UINT status = tx_thread_create(&motor_control_thread, "motor_control_task", motor_control_task,
                                   0, &motor_control_thread_stack, MOTOR_CONTROL_THREAD_STACK_SIZE,
                                   MOTOR_CONTROL_THREAD_PRIORITY, MOTOR_CONTROL_THREAD_PRIORITY,
                                   TX_NO_TIME_SLICE, TX_AUTO_START);
    status      = tx_thread_create(&motor_decode_thread, "motor_decode_task", motor_decode_task, 0,
                                   &motor_decode_thread_stack, MOTOR_DECODE_THREAD_STACK_SIZE,
                                   MOTOR_DECODE_THREAD_PRIORITY, MOTOR_DECODE_THREAD_PRIORITY,
                                   TX_NO_TIME_SLICE, TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        log_e("Failed to create motor task!");
        return;
    }

    log_i("motor task started successfully.");
}