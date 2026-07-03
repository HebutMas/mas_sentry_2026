#include "module_itc.h"
#include <stdio.h>
#include <string.h>

#define LOG_TAG "module_itc"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

// 全局变量
static itc_topic_t      *topic_list      = NULL;
static itc_subscriber_t *subscriber_list = NULL;

// 内部函数实现
static int find_topic(const char *topic_name, itc_topic_t **topic)
{
    itc_topic_t *current = topic_list;
    while (current != NULL)
    {
        if (strcmp(current->name, topic_name) == 0)
        {
            *topic = current;
            return 0;
        }
        current = current->next;
    }
    return -1;
}

static int add_topic_to_list(itc_topic_t *topic)
{
    topic->next = topic_list;
    topic_list  = topic;
    return 0;
}

static int remove_topic_from_list(itc_topic_t *topic)
{
    if (topic_list == topic)
    {
        topic_list = topic->next;
        return 0;
    }
    itc_topic_t *current = topic_list;
    while (current != NULL && current->next != topic)
    {
        current = current->next;
    }
    if (current != NULL)
    {
        current->next = topic->next;
        return 0;
    }
    return -1;
}

static int add_subscriber_to_list(itc_subscriber_t *subscriber)
{
    subscriber->next = subscriber_list;
    subscriber_list  = subscriber;
    return 0;
}

static int remove_subscriber_from_list(itc_subscriber_t *subscriber)
{
    if (subscriber_list == subscriber)
    {
        subscriber_list = subscriber->next;
        return 0;
    }
    itc_subscriber_t *current = subscriber_list;
    while (current != NULL && current->next != subscriber)
    {
        current = current->next;
    }
    if (current != NULL)
    {
        current->next = subscriber->next;
        return 0;
    }
    return -1;
}

// 对外接口
UINT Module_Itc_topic_init(itc_topic_t *topic, const char *topic_name, uint32_t data_len)
{
    itc_topic_t *existing_topic;

    if (topic == NULL || topic_name == NULL || strlen(topic_name) >= ITC_MAX_TOPIC_NAME_LEN ||
        data_len == 0)
    {
        log_e("Invalid topic parameters");
        return TX_DELETED;
    }

    if (find_topic(topic_name, &existing_topic) == 0)
    {
        log_e("Topic %s already exists", topic_name);
        return TX_DELETED;
    }

    memset(topic, 0, sizeof(itc_topic_t));
    strcpy(topic->name, topic_name);
    topic->is_active          = 1;
    topic->publisher_data_len = data_len;
    topic->subscriber_ptr     = NULL;

    add_topic_to_list(topic);

    // 尝试绑定等待中的订阅者
    itc_subscriber_t *sub = subscriber_list;
    while (sub != NULL)
    {
        // 如果订阅者未绑定(is_active=1但topic_ptr=NULL) 且 目标话题名称匹配
        if (sub->is_active && sub->topic_ptr == NULL &&
            strcmp(sub->target_topic_name, topic_name) == 0)
        {
            // 检查数据长度是否匹配
            if (sub->expected_data_len != topic->publisher_data_len)
            {
                log_e("Pending subscriber %s data length mismatch for topic %s", sub->name,
                      topic_name);
                sub = sub->next;
                continue; // 长度不匹配，跳过
            }

            // 检查话题是否已被占用
            if (topic->subscriber_ptr == NULL)
            {
                // 绑定成功
                sub->topic_ptr        = topic;
                topic->subscriber_ptr = sub;
                log_i("Auto-bound pending subscriber %s to topic %s", sub->name, topic_name);
                break; // 1对1模式，绑定一个即停止
            }
        }
        sub = sub->next;
    }

    return TX_SUCCESS;
}

// 反初始化话题
UINT Module_Itc_topic_deinit(itc_topic_t *topic)
{
    if (topic == NULL)
    {
        log_e("Attempt to deinit NULL topic");
        return TX_DELETED;
    }

    if (topic->subscriber_ptr != NULL)
    {
        topic->subscriber_ptr->topic_ptr        = NULL;
        topic->subscriber_ptr->current_data_ptr = NULL;
        // 注意：这里不把subscriber设为inactive，允许它等待话题重新创建
    }
    topic->subscriber_ptr = NULL;

    remove_topic_from_list(topic);
    return TX_SUCCESS;
}

UINT Module_Itc_subscriber_init(itc_subscriber_t *subscriber, const char *topic_name,
                                uint32_t expected_data_len)
{
    itc_topic_t *topic;

    if (subscriber == NULL || topic_name == NULL || expected_data_len == 0)
    {
        log_e("Invalid subscriber parameters");
        return TX_DELETED;
    }

    if (strlen(topic_name) >= ITC_MAX_TOPIC_NAME_LEN)
    {
        log_e("Topic name too long");
        return TX_DELETED;
    }

    memset(subscriber, 0, sizeof(itc_subscriber_t));
    subscriber->is_active         = 1;
    subscriber->expected_data_len = expected_data_len;

    // 保存目标话题名称，用于延迟查找
    strcpy(subscriber->target_topic_name, topic_name);

    // 尝试立即查找话题
    if (find_topic(topic_name, &topic) == 0)
    {
        // 话题存在，执行立即绑定逻辑
        // 检查是否已被占用 (一对一原则)
        if (topic->subscriber_ptr != NULL)
        {
            log_e("Topic %s already has a subscriber (1-to-1 mode)", topic_name);
            return TX_DELETED;
        }

        // 检查数据长度
        if (expected_data_len != topic->publisher_data_len)
        {
            log_e("Data length mismatch for topic %s", topic_name);
            return TX_DELETED;
        }

        // 绑定
        subscriber->topic_ptr = topic;
        topic->subscriber_ptr = subscriber;
        log_i("Subscriber %s bound to topic %s immediately", subscriber->name, topic_name);
    }
    else
    {
        // 话题不存在，进入延迟绑定模式 (Pending状态)，topic_ptr 保持为 NULL
        // 注意：这里返回 TX_SUCCESS，因为订阅者初始化本身是成功的，只是处于等待状态
        log_w("Topic %s not found, subscriber %s entering pending mode", topic_name,
              subscriber->name);
    }

    snprintf(subscriber->name, sizeof(subscriber->name), "sub_%p", subscriber);

    add_subscriber_to_list(subscriber);

    return TX_SUCCESS;
}

UINT Module_Itc_subscriber_deinit(itc_subscriber_t *subscriber)
{
    if (subscriber == NULL)
    {
        log_e("Attempt to deinit NULL subscriber pointer");
        return TX_DELETED;
    }

    remove_subscriber_from_list(subscriber);

    if (subscriber->topic_ptr != NULL)
    {
        subscriber->topic_ptr->subscriber_ptr = NULL;
    }

    subscriber->is_active        = 0;
    subscriber->topic_ptr        = NULL;
    subscriber->current_data_ptr = NULL;

    // 清除目标话题名称
    memset(subscriber->target_topic_name, 0, sizeof(subscriber->target_topic_name));

    return TX_SUCCESS;
}

UINT Module_Itc_publish(itc_topic_t *topic, void *data_ptr)
{
    if (!topic || !data_ptr || !topic->is_active)
    {
        log_e("Invalid parameters for publish");
        return TX_DELETED;
    }

    if (topic->subscriber_ptr == NULL)
    {
        return TX_SUCCESS;
    }

    itc_subscriber_t *subscriber = topic->subscriber_ptr;
    if (subscriber->is_active)
    {
        // 直接赋值
        subscriber->current_data_ptr = data_ptr;
    }

    return TX_SUCCESS;
}

void *Module_Itc_receive(itc_subscriber_t *subscriber)
{
    if (!subscriber || !subscriber->is_active)
    {
        return NULL;
    }

    // 如果 topic_ptr 为 NULL (未绑定/等待中)，直接返回 NULL
    if (subscriber->topic_ptr == NULL)
    {
        return NULL;
    }

    // 直接返回当前指针
    return (void *)subscriber->current_data_ptr;
}