/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-11 19:45:56
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-27 19:27:09
 * @FilePath: /MAS_RM_DJIC/Mas_User/MODULES/OFFLINE/module_offline.h
 * @Description:
 */
#ifndef _OFFLINE_H_
#define _OFFLINE_H_

#include "stdbool.h"
#include "tx_port.h"
#include <stdint.h>

// 宏定义
#define OFFLINE_WATCHDOG_ENABLE 1 // 启用离线检测看门狗功能
#define OFFLINE_BEEP_ENABLE     1 // 开启离线蜂鸣器功能


#if defined(STM32H723xx)
#define OFFLINE_BEEP_TUNE_VALUE 100 // 这两个部分决定beep的音调，音色
#define OFFLINE_BEEP_CTRL_VALUE 100
#elif defined(STM32F407xx)
#define OFFLINE_BEEP_TUNE_VALUE 50 // 这两个部分决定beep的音调，音色
#define OFFLINE_BEEP_CTRL_VALUE 100
#endif

// 状态定义
#define STATE_ONLINE            0
#define STATE_OFFLINE           1
#define OFFLINE_ENABLE          1
#define OFFLINE_DISABLE         0
#define OFFLINE_INVALID_INDEX   0xFF

// 设备初始化配置结构体
typedef struct
{
    const char *name;       // 设备名称
    uint32_t    timeout_ms; // 超时时间
    uint8_t     beep_times; // 蜂鸣次数
    uint8_t     enable;     // 是否启用检测
} Offline_Init_config_t;

// 离线设备结构体
typedef struct OfflineDevice
{
    uint32_t              valid;      // 设备指针有效性检测标志
    char                  name[32];   // 名称
    uint32_t              timeout_ms; // 超时时间
    uint8_t               beep_times; // 蜂鸣次数
    bool                  is_offline; // 是否离线
    uint32_t              last_time;  // 上次更新时间
    uint8_t               enable;     // 是否启用检测
    struct OfflineDevice *next;       // 链表下一个节点指针
} OfflineDevice;

// 离线管理器结构体
typedef struct
{
    OfflineDevice *device_list;  // 设备链表头指针
    uint8_t        device_count; // 设备数量
    bool           initialized;  // 初始化状态
    volatile bool  silent_error; // 静音报警，针对beep_times=0的情况
} OfflineManager_t;

// 函数声明
/**
 * @brief 离线模块初始化
 */
void Module_Offline_init(void);
/**
 * @brief 注册一个新的离线检测设备
 * @param init 设备初始化配置结构体指针
 * @return 成功返回设备结构体指针(句柄)，失败返回NULL
 */
OfflineDevice *Module_Offline_device_register(const Offline_Init_config_t *init);
/**
 * @brief 注销一个离线检测设备
 * @param dev 设备结构体指针(句柄)
 * @return 无
 */
void Module_Offline_device_unregister(OfflineDevice *dev);
/**
 * @brief 更新设备的心跳时间
 * @param dev 设备结构体指针(句柄)
 * @return 无
 */
void Module_Offline_device_update(OfflineDevice *dev);
/**
 * @brief 开启指定设备的离线检测功能
 * @param dev 设备结构体指针(句柄)
 * @return 无
 */
void Module_Offline_device_enable(OfflineDevice *dev);
/**
 * @brief 关闭指定设备的离线检测功能
 * @param dev 设备结构体指针(句柄)
 * @return 无
 */
void Module_Offline_device_disable(OfflineDevice *dev);
/**
 * @brief 获取指定设备的离线状态
 * @param dev 设备结构体指针(句柄)
 * @return STATE_ONLINE(在线)，STATE_OFFLINE(离线或指针无效)
 */
uint8_t Module_Offline_get_device_status(OfflineDevice *dev);
/**
 * @brief 离线检测主任务函数
 * @details 需要在循环中周期性调用，用于遍历设备链表检查超时、更新离线状态、
 */
void Module_Offline_task_function(void);
/**
 * @description: 离线检测定时器任务
 * @details 需要在定时器任务中循环调用，或者与task放在一个线程调用也可
 * @param {ULONG} timer
 * @return {*}
 */
void Module_offline_beep_ctrl_times(ULONG timer);

#endif // _OFFLINE_H_