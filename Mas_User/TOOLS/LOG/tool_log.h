/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 10:28:44
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-13 14:47:43
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/TOOLS/LOG/tool_log.h
 * @Description: Log output function declarations - 支持多线程互斥
 */
#ifndef _TOOL_LOG_H_
#define _TOOL_LOG_H_

#include <stdarg.h>

/* 日志等级 */
#define LOG_LVL_ERROR     3
#define LOG_LVL_WARNING   4
#define LOG_LVL_INFO      6
#define LOG_LVL_DBG       7

/* 日志缓冲区大小*/
#define LOG_LINE_BUF_SIZE 256
/* 输出日志等级(全局) */
#define LOG_OUTPUT_LVL    LOG_LVL_DBG
/* 日志tag */
#if !defined(LOG_TAG)
#define LOG_TAG "NO_TAG"
#endif
/* 日志输出等级(单个文件)*/
#if !defined(LOG_LVL)
#define LOG_LVL LOG_LVL_DBG
#endif

#if (LOG_LVL >= LOG_LVL_DBG) && (LOG_OUTPUT_LVL >= LOG_LVL_DBG)
#define log_d(...) log_output(LOG_LVL_DBG, LOG_TAG, __VA_ARGS__)
#else
#define log_d(...)
#endif

#if (LOG_LVL >= LOG_LVL_INFO) && (LOG_OUTPUT_LVL >= LOG_LVL_INFO)
#define log_i(...) log_output(LOG_LVL_INFO, LOG_TAG, __VA_ARGS__)
#else
#define log_i(...)
#endif

#if (LOG_LVL >= LOG_LVL_WARNING) && (LOG_OUTPUT_LVL >= LOG_LVL_WARNING)
#define log_w(...) log_output(LOG_LVL_WARNING, LOG_TAG, __VA_ARGS__)
#else
#define log_w(...)
#endif

#if (LOG_LVL >= LOG_LVL_ERROR) && (LOG_OUTPUT_LVL >= LOG_LVL_ERROR)
#define log_e(...) log_output(LOG_LVL_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define log_e(...)
#endif

/**
 * @brief 初始化日志系统
 * @return int 0-成功, -1-失败
 */
int log_init(void);

/**
 * @brief LOG 输出函数（使用RTT）
 * @param level log等级
 * @param tag log tag
 * @param fmt 格式化字符串
 * @param ... 变量参数
 */
void log_output(int level, const char *tag, const char *fmt, ...);

#endif // _TOOL_LOG_H_
