/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 14:23:51
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-28 03:07:13
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/BSP/bsp_def.h
 * @Description:
 */
#ifndef _BSP_DEF_H_
#define _BSP_DEF_H_

#include "tx_api.h"
extern TX_BYTE_POOL tx_app_byte_pool;

/* h7与f4 数据和线程栈等SECTION宏定义 */
#if defined(STM32H723xx)
#define APPS_STACK_SECTION __attribute__((section(".dtcmram")))
#define BUFFER_SECTION     __attribute__((section(".RAM_D1")))
#define CODE_RAM_SECTION   __attribute__((section(".itcmram")))
#elif defined(STM32F407xx)
#define APPS_STACK_SECTION __attribute__((section(".ccmram")))
#define BUFFER_SECTION     __attribute__((section(".ram")))
#define CODE_RAM_SECTION   __attribute__((section(".ccmram")))
#endif

/* 内存分配和释放宏定义 */
/**
 * @brief 从应用字节池分配内存（带等待超时）
 * @param ptr 内存指针变量（将被设置为分配的地址）
 * @param size 要分配的字节数
 * @param wait_option 等待选项（TX_NO_WAIT 或超时值）
 * @return TX_SUCCESS 成功，其他值表示失败
 */
#define BSP_MEM_ALLOC_WAIT(ptr, size, wait_option)                                                 \
    do                                                                                             \
    {                                                                                              \
        UINT _status;                                                                              \
        _status = tx_byte_allocate(&tx_app_byte_pool, (void **)&(ptr), (size), (wait_option));     \
        if (_status != TX_SUCCESS)                                                                 \
        {                                                                                          \
            (ptr) = NULL;                                                                          \
        }                                                                                          \
    } while (0)

/**
 * @brief 释放内存到应用字节池
 * @param ptr 要释放的内存指针
 * @return TX_SUCCESS 成功，其他值表示失败
 */
#define BSP_MEM_FREE(ptr)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if ((ptr) != NULL)                                                                         \
        {                                                                                          \
            tx_byte_release((void *)(ptr));                                                        \
            (ptr) = NULL;                                                                          \
        }                                                                                          \
    } while (0)

#endif // _BSP_DEF_H_
