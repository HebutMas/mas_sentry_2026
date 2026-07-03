#ifndef _MODULE_ITC_H_
#define _MODULE_ITC_H_

#include "tx_api.h"
#include <stdbool.h>
#include <stdint.h>

// 配置参数
#define ITC_MAX_TOPIC_NAME_LEN 32 // 最大话题名称长度

// 前向声明
typedef struct itc_topic      itc_topic_t;
typedef struct itc_subscriber itc_subscriber_t;

// 话题结构体 (1对1)
struct itc_topic
{
    char              name[ITC_MAX_TOPIC_NAME_LEN]; // 话题名称
    volatile uint8_t  is_active;                    // 是否激活
    uint8_t           publisher_data_len;           // 发布者规定的数据长度
    itc_subscriber_t *subscriber_ptr;               // 关联的订阅者指针 (一对一)
    itc_topic_t      *next;                         // 下一个话题
};

// 订阅者结构体
struct itc_subscriber
{
    volatile uint8_t  is_active;         // 是否激活
    itc_subscriber_t *next;              // 下一个订阅者 (用于全局链表)
    itc_topic_t      *topic_ptr;         // 关联的话题指针 (为NULL表示未绑定/等待中)
    uint32_t          expected_data_len; // 订阅者期望的数据长度

    volatile void *current_data_ptr; // 指向最新数据的指针 (由发布者更新，接收者直接读)

    char name[32];                                  // 订阅者名称
    char target_topic_name[ITC_MAX_TOPIC_NAME_LEN]; // 目标话题名称，用于延迟绑定查找
};

/**
 * @brief 初始化一个话题 (1对1模式)
 */
UINT Module_Itc_topic_init(itc_topic_t *topic, const char *topic_name, uint32_t data_len);

/**
 * @brief 反初始化一个话题
 */
UINT Module_Itc_topic_deinit(itc_topic_t *topic);

/**
 * @brief 初始化一个订阅者 (支持延迟绑定：如果话题不存在，进入等待状态)
 */
UINT Module_Itc_subscriber_init(itc_subscriber_t *subscriber, const char *topic_name,
                                uint32_t expected_data_len);

/**
 * @brief 反初始化一个订阅者
 */
UINT Module_Itc_subscriber_deinit(itc_subscriber_t *subscriber);

/**
 * @brief 发布消息 (直接更新指针)
 */
UINT Module_Itc_publish(itc_topic_t *topic, void *data_ptr);

/**
 * @brief 接收消息
 */
void *Module_Itc_receive(itc_subscriber_t *subscriber);

#endif // _MODULE_ITC_H_