/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 10:28:44
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-13 21:30:06
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/TOOLS/LOG/tool_log.c
 * @Description: Log output implementation using RTT - 支持多线程互斥和ISR自动检测
 */

#include "tool_log.h"
#include "SEGGER_RTT.h"
#include "bsp_dwt.h"
#include "tx_api.h"
#include <stdio.h>

/* 日志互斥量 */
static TX_MUTEX log_mutex;
/* 互斥量是否已初始化 */
static uint8_t log_mutex_initialized = 0;

/**
 * @brief 检查当前是否在中断上下文中
 * @return int 1-在ISR中, 0-不在ISR中
 */
static int log_is_in_isr(void)
{
    /* 使用ThreadX API检测 */
    TX_THREAD *current_thread = tx_thread_identify();

    /* 如果返回NULL，说明在ISR中 */
    return (current_thread == NULL) ? 1 : 0;
}

// 每个级别颜色编码
static const char *LOG_LEVEL_COLOR[] = {
    "", // No level
    "",         "",
    "\033[31m", // ERROR - Red
    "\033[33m", // WARNING - Yellow
    "",
    "\033[32m", // INFO - Green
    "\033[36m", // DBG - Cyan
};

// 日志等级字符串
static const char *LOG_LEVEL_STR[] = {"", "", "", "ERROR", "WARN", "", "INFO", "DBG"};

int log_init(void)
{
    UINT status;
    status = _tx_mutex_create(&log_mutex, "Log Mutex", TX_NO_INHERIT);

    if (status == TX_SUCCESS)
    {
        log_mutex_initialized = 1;
        return 0;
    }
    else
    {
        log_mutex_initialized = 0;
        return -1;
    }
}

void log_output(int level, const char *tag, const char *fmt, ...)
{
    static char log_buf[LOG_LINE_BUF_SIZE];
    va_list     args;
    int         len;
    UINT        status;
    int         in_isr;

    // 检查是否合法
    if (level < LOG_LVL_ERROR || level > LOG_LVL_DBG || log_mutex_initialized == 0)
    {
        return;
    }

    // 检测是否在ISR中
    in_isr = log_is_in_isr();

    // 获取互斥量，仅在非ISR环境中
    if (!in_isr)
    {
        status = tx_mutex_get(&log_mutex, TX_WAIT_FOREVER);

        if (status != TX_SUCCESS)
        {
            return;
        }
    }

    len = 0;
    // 先设置颜色
    if (level >= 0 && level <= 7)
    {
        len += snprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, "%s", LOG_LEVEL_COLOR[level]);
    }

    // 添加时间戳 [MM:SS.mmm.uuu] (使用DWT高精度时间)
    uint64_t time_us = DWT_GetTimeline_us();
    uint32_t minutes = (time_us / 60000000ULL) % 60; // 分
    uint32_t seconds = (time_us / 1000000ULL) % 60;  // 秒
    uint32_t millis  = (time_us / 1000ULL) % 1000;   // 毫秒
    len += snprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, "[%02lu:%02lu.%03lu]", minutes, seconds,
                    millis);

    // 添加level
    if (level >= 0 && level <= 7)
    {
        len += snprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, "[%s]", LOG_LEVEL_STR[level]);
    }

    // 添加tag
    if (tag != NULL)
    {
        len += snprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, "[%s]", tag);
    }

    // 添加单个空格分隔
    len += snprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, " ");

    // 添加消息
    va_start(args, fmt);
    len += vsnprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, fmt, args);
    va_end(args);

    // 重置颜色
    len += snprintf(log_buf + len, LOG_LINE_BUF_SIZE - len, "\033[0m");

    // 设置为新行
    if (len < LOG_LINE_BUF_SIZE - 2)
    {
        log_buf[len++] = '\n';
        log_buf[len++] = '\0';
    }

    SEGGER_RTT_WriteString(0, log_buf);

    // 释放互斥量,仅在非ISR环境中
    if (!in_isr)
    {
        tx_mutex_put(&log_mutex);
    }
}
