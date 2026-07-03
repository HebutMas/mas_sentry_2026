/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-08 18:07:13
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-28 03:01:00
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/BSP/DWT/bsp_dwt.h
 * @Description:
 */
#ifndef _BSP_DWT_H_
#define _BSP_DWT_H_

#include "stdint.h"
#if defined(STM32H723xx)
#include "stm32h723xx.h"

#define DWT_MEASURE_START() \
    do { \
        uint32_t __dwt_start_time = DWT->CYCCNT; \
        
#define DWT_MEASURE_END(function_name) \
        uint32_t __dwt_end_time = DWT->CYCCNT; \
        uint32_t __dwt_cycles = __dwt_end_time - __dwt_start_time; \
        float __dwt_us_time = (float)__dwt_cycles / (480000000 / 1000000.0f); \
        log_d("[PERF] %s: %lu cycles, %.2f us", function_name, __dwt_cycles, __dwt_us_time); \
    } while(0)
#elif defined(STM32F407xx)
#include "stm32f407xx.h"

#define DWT_MEASURE_START() \
    do { \
        uint32_t __dwt_start_time = DWT->CYCCNT; \
        
#define DWT_MEASURE_END(function_name) \
        uint32_t __dwt_end_time = DWT->CYCCNT; \
        uint32_t __dwt_cycles = __dwt_end_time - __dwt_start_time; \
        float __dwt_us_time = (float)__dwt_cycles / (168000000 / 1000000.0f); \
        log_d("[PERF] %s: %lu cycles, %.2f us", function_name, __dwt_cycles, __dwt_us_time); \
    } while(0)
#endif



typedef struct
{
    uint32_t s;
    uint16_t ms;
    uint16_t us;
} DWT_Time_t;

/**
 * @brief 初始化DWT,传入参数为CPU频率,单位MHz
 *
 * @param CPU_Freq_mHz c板为168MHz,A板为180MHz,达妙h7为480MHz
 */
void DWT_Init(uint32_t CPU_Freq_mHz);

/**
 * @brief 获取两次调用之间的时间间隔,单位为秒/s
 *
 * @param cnt_last 上一次调用的时间戳
 * @return float 时间间隔,单位为秒/s
 */
float DWT_GetDeltaT(uint32_t *cnt_last);

/**
 * @brief 获取两次调用之间的时间间隔,单位为秒/s,高精度
 *
 * @param cnt_last 上一次调用的时间戳
 * @return double 时间间隔,单位为秒/s
 */
double DWT_GetDeltaT64(uint32_t *cnt_last);

/**
 * @brief 获取当前时间,单位为秒/s,即初始化后的时间
 *
 * @return float 时间轴
 */
float DWT_GetTimeline_s(void);

/**
 * @brief 获取当前时间,单位为毫秒/ms,即初始化后的时间
 *
 * @return float
 */
float DWT_GetTimeline_ms(void);

/**
 * @brief 获取当前时间,单位为微秒/us,即初始化后的时间
 *
 * @return uint64_t
 */
uint64_t DWT_GetTimeline_us(void);

/**
 * @brief DWT延时函数,单位为秒/s
 * @attention 该函数不受中断是否开启的影响,可以在临界区和关闭中断时使用
 * @note 禁止在rtos初始化完成之前或者中断临界区使用HAL_Delay(),tx_thread_sleep()函数,应使用本函数
 * @param Delay 延时时间,单位为秒/s
 */
void DWT_Delay(float Delay);

/**
 * @brief DWT更新时间轴函数,会被三个timeline函数调用
 * @attention
 * 如果长时间不调用timeline函数,则需要手动调用该函数更新时间轴,否则CYCCNT溢出后定时和时间轴不准确
 */
void DWT_SysTimeUpdate(void);

#endif // _BSP_DWT_H_