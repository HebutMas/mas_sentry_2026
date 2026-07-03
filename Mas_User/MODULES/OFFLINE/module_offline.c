/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-11 19:45:50
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-28 02:34:09
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/OFFLINE/module_offline.c
 * @Description:
 */
#include "module_offline.h"
#include "bsp_def.h"
#include "bsp_dwt.h"
#include "module_beep.h"
#include "module_rgb.h"
#include <stdint.h>
#include <string.h>

#define LOG_TAG "module_offline"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

#define VALID_CKECK 0xABCDEF01

static OfflineManager_t offline_manager;
static volatile uint8_t current_beep_times = 0;

void Module_Offline_init(void)
{
    // 初始化功能相关配置
    memset(&offline_manager, 0, sizeof(OfflineManager_t));
    offline_manager.initialized  = true;
    offline_manager.silent_error = false;

    // 初始化RGB
    Module_Rgb_init();

    // 初始化Beep模块
    Module_Beep_init();
    Module_Beep_Start();

    log_i("MODULE OFFLINE init success.");
}

OfflineDevice *Module_Offline_device_register(const Offline_Init_config_t *init)
{
    if (init == NULL || offline_manager.initialized == false)
    {
        return NULL;
    }

    OfflineDevice *dev = NULL;
    BSP_MEM_ALLOC_WAIT(dev, sizeof(OfflineDevice), TX_NO_WAIT);
    if (dev == NULL)
    {
        log_e("Failed to allocate memory for Offlinedevice");
        return NULL;
    }

    memset(dev, 0, sizeof(OfflineDevice));

    strncpy(dev->name, init->name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->timeout_ms                  = init->timeout_ms;
    dev->beep_times                  = init->beep_times;
    dev->is_offline                  = STATE_OFFLINE;
    dev->last_time                   = 0;
    dev->enable                      = init->enable;
    dev->valid                       = VALID_CKECK;

    // 插入链表
    dev->next                   = offline_manager.device_list;
    offline_manager.device_list = dev;
    offline_manager.device_count++;

    log_i("offline device register: %s", dev->name);

    return dev;
}

void Module_Offline_device_unregister(OfflineDevice *dev)
{
    if (dev == NULL || offline_manager.initialized == false)
    {
        return;
    }
    OfflineDevice *curr = offline_manager.device_list;
    OfflineDevice *prev = NULL;

    while (curr != NULL)
    {
        if (curr == dev)
        {
            if (prev == NULL)
            {
                offline_manager.device_list = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }
            offline_manager.device_count--;

            curr->valid = 0x00000000;
            BSP_MEM_FREE(curr);

            log_i("offline device unregister: %s", curr->name);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void Module_Offline_device_update(OfflineDevice *dev)
{
    if (dev != NULL && offline_manager.initialized == true && dev->valid == VALID_CKECK)
    {
        dev->last_time = DWT_GetTimeline_ms();
    }
}

void Module_Offline_device_enable(OfflineDevice *dev)
{
    if (dev != NULL && offline_manager.initialized == true && dev->valid == VALID_CKECK)
    {
        dev->enable = OFFLINE_ENABLE;
    }
}

void Module_Offline_device_disable(OfflineDevice *dev)
{
    if (dev != NULL && offline_manager.initialized == true && dev->valid == VALID_CKECK)
    {
        dev->enable = OFFLINE_DISABLE;
    }
}

uint8_t Module_Offline_get_device_status(OfflineDevice *dev)
{
    if (dev != NULL && offline_manager.initialized == true && dev->valid == VALID_CKECK)
    {
        return dev->is_offline;
    }
    return STATE_OFFLINE;
}

void Module_Offline_task_function(void)
{
    if (offline_manager.initialized == true)
    {
        uint32_t current_time = (uint32_t)DWT_GetTimeline_ms();

        bool    has_severe_fault   = false; // 是否存在“需要蜂鸣”的离线设备 (高优先级)
        bool    has_silent_fault   = false; // 是否存在“静音”的离线设备 (低优先级)
        uint8_t request_beep_times = 0;     // 保存高优先级下最小的蜂鸣次数

        OfflineDevice *dev = offline_manager.device_list;
        while (dev != NULL)
        {
            OfflineDevice *next_dev = dev->next;
            if (dev->enable)
            {
                if (current_time - dev->last_time > dev->timeout_ms)
                {
                    dev->is_offline = STATE_OFFLINE;
                    if (dev->beep_times > 0)
                    {
                        has_severe_fault = true;
                        // 寻找次数最少的
                        if (request_beep_times == 0 || dev->beep_times < request_beep_times)
                        {
                            request_beep_times = dev->beep_times;
                        }
                    }
                    else if (dev->beep_times == 0)
                    {
                        has_silent_fault = true;
                    }
                }
                else
                {
                    dev->is_offline = STATE_ONLINE;
                }
            }

            dev = next_dev;
        }

        if (has_severe_fault)
        {
            offline_manager.silent_error = false;
            current_beep_times           = request_beep_times;
        }
        else if (has_silent_fault)
        {
            offline_manager.silent_error = true;
            current_beep_times           = 0;
        }
        else
        {
            offline_manager.silent_error = false;
            current_beep_times           = 0;
        }
    }
    else
    {
        while (1)
        {
            log_e("The module has not been initialized, so the thread function cannot run");
        }
    }
}

void Module_offline_beep_ctrl_times(ULONG timer)
{
    static uint32_t beep_period_start_time;
    static uint32_t beep_cycle_start_time;
    static uint8_t  remaining_beep_cycles;

    if (offline_manager.initialized == true)
    {
        uint32_t now = DWT_GetTimeline_ms();

        // 静音故障模式 (红灯常亮)
        if (offline_manager.silent_error)
        {
            Module_Beep_set(0, 0);
            Module_Rgb_show(LED_Red);

            // 重置状态机变量，防止切回闪烁模式时状态错乱
            remaining_beep_cycles  = 0;
            beep_period_start_time = now;
            return;
        }

        // 闪烁模式 (红灯闪烁)，每2秒为一个大周期
        if (now - beep_period_start_time > 2000)
        {
            remaining_beep_cycles  = current_beep_times;
            beep_period_start_time = now;
            beep_cycle_start_time  = now;
        }

        if (remaining_beep_cycles != 0)
        {
            // 0-100ms: 亮/响
            if (now - beep_cycle_start_time < 100)
            {
#if OFFLINE_BEEP_ENABLE == 1
                Module_Beep_set(OFFLINE_BEEP_TUNE_VALUE, OFFLINE_BEEP_CTRL_VALUE);
#else
                Module_Beep_set(0, 0);
#endif
                // 无论Beep是否开启，LED都必须闪烁
                Module_Rgb_show(LED_Red);
            }
            // 100-200ms: 灭/停
            else if (now - beep_cycle_start_time < 200)
            {
                Module_Beep_set(0, 0);
                Module_Rgb_show(LED_Black);
            }
            // >200ms: 进入下一次滴答倒计时
            else
            {
                remaining_beep_cycles--;
                beep_cycle_start_time = now;
            }
        }
        else
        {
            if (current_beep_times > 0)
            {
                Module_Beep_set(0, 0);
                Module_Rgb_show(LED_Black); // 故障间隙保持黑灯
            }
            else
            {
                Module_Beep_set(0, 0);
                Module_Rgb_show(LED_Green); // 一切正常亮绿灯
            }
        }
    }
    else
    {
        Module_Beep_set(0, 0);
        Module_Rgb_show(LED_Black);
    }
}