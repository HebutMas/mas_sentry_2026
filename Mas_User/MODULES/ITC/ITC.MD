# 模块层 ITC (Inter-Thread Communication) 模块文档

## 1. 概述

ITC (Inter-Thread Communication) 模块是一个轻量级的、基于零拷贝思想的线程间通信组件。它通过“发布-订阅”模型实现数据在不同线程间的传递。为了保证的运行效率，该模块采用直接指针交换，允许订阅者在话题创建之前先行注册。

---

## 2. 架构设计

### 2.1 数据结构

#### itc_topic_t - 话题结构体

```c
struct itc_topic
{
    char              name[ITC_MAX_TOPIC_NAME_LEN]; // 话题唯一标识名称
    volatile uint8_t  is_active;                    // 激活状态
    uint8_t           publisher_data_len;           // 校验用数据长度
    itc_subscriber_t *subscriber_ptr;               // 绑定的订阅者指针
    itc_topic_t      *next;                         // 全局话题链表指针
};
```
#### itc_subscriber_t - 订阅者结构体

订阅者通过话题名称进行绑定，并直接访问发布者提供的数据地址。

```c
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

```

### 2.2 通信逻辑说明

* 零拷贝机制：`Module_Itc_publish` 不搬运内存数据，仅将发布者本地变量的地址赋值给订阅者的 `current_data_ptr`。
* 延迟绑定：如果订阅者在初始化时目标话题尚未创建，它将被加入全局链表。当后续有同名话题初始化时，系统会自动完成关联。
* 一对一原则：为了简化逻辑并保证实时性，目前一个话题仅允许绑定一个订阅者。

### 2.3 代码架构

```c
内部辅助函数层 (Static)：
├─ find_topic()                 // 遍历链表查找指定名称话题
├─ add_topic_to_list()          // 话题注册进全局链表
├─ remove_topic_from_list()     // 话题移除全局链表
├─ add_subscriber_to_list()     // 订阅者注册进全局链表
└─ remove_subscriber_from_list()// 订阅者移除全局链表

公开 API 层：
├─ Module_Itc_topic_init()      // 创建话题并尝试绑定延迟的订阅者
├─ Module_Itc_topic_deinit()      // 反初始化话题
├─ Module_Itc_subscriber_init() // 创建订阅者并尝试立即/延迟绑定
├─ Module_Itc_subscriber_deinit() // 反初始化订阅者
├─ Module_Itc_publish()         // 发布数据 (指针赋值)
└─ Module_Itc_receive()         // 获取当前话题的数据指针

```

---

## 3. API 接口说明

### 3.1 话题管理

```c
UINT Module_Itc_topic_init(itc_topic_t *topic, const char *topic_name, uint32_t data_len);
UINT Module_Itc_topic_deinit(itc_topic_t *topic);

```
### 3.2 订阅管理

```c
UINT Module_Itc_subscriber_init(itc_subscriber_t *subscriber, const char *topic_name, uint32_t expected_data_len);

```
### 3.3 数据交互

```c
UINT Module_Itc_publish(itc_topic_t *topic, void *data_ptr);
void *Module_Itc_receive(itc_subscriber_t *subscriber);

```
---

## 4. 注意事项

- 生命周期安全：由于是零拷贝通信，发布者通过 `Module_Itc_publish` 传递的地址必须在订阅者处理完成前保持有效。
- 内存分配：本模块不负责结构体的内存申请，`itc_topic_t` 和 `itc_subscriber_t` 应由用户在静态区或通过堆分配后传入。
- 长度校验：初始化时的 `data_len` 仅用于基本的通讯契约校验，防止类型误用。

---

## 5. 使用示例

### 发布者 (如robot_control线程)
```c
static itc_topic_t       gimbal_cmd_topic;
static Gimbal_Ctrl_Cmd_s gimbal_cmd;

void App_robot_control_Init(void)
{
    status =
        Module_Itc_topic_init(&gimbal_cmd_topic, "gimbal_cmd_topic", sizeof(Gimbal_Ctrl_Cmd_s));//初始化主题
    if (status != TX_SUCCESS)
    {
        log_e("gimbal topic init failed!");
        return;
    }

}
void control_task(void)
{
    while(1) {
        Module_Itc_publish(&gimbal_cmd_topic, &gimbal_cmd);//发布指令
        tx_thread_sleep(2);
    }
}
```
### 订阅者 (如gimbal线程)

```c
static itc_subscriber_t   gimbal_sub;
static Gimbal_Ctrl_Cmd_s *gimbal_cmd = NULL;

void App_gimbal_Init(void)
{
        status =
        Module_Itc_subscriber_init(&gimbal_sub, "gimbal_cmd_topic", sizeof(Gimbal_Ctrl_Cmd_s));//初始化订阅者
    if (status != TX_SUCCESS)
    {
        log_e("gimbal topic init failed!");
        return;
    }

}
void gimbal_task(void)
{
    gimbal_cmd = (Gimbal_Ctrl_Cmd_s *)Module_Itc_receive(&gimbal_sub);
}
```
---

## 6. 版本历史

### v1.0 (2026-02-12)

- 初始版本

---
