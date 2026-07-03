# BSP_Offline 离线检测模块文档 
## 1. 概述 
适用于STM32系列MCU的通用离线检测管理模块。

## 2. 架构设计 

### 2.1 数据结构 

#### OfflineDevice - 离线设备实例 
```c
typedef struct OfflineDevice {
    uint32_t              valid;      // 设备有效性检测标志 (VALID_CKECK)
    char                  name[32];   // 设备名称
    uint32_t              timeout_ms; // 超时时间阈值
    uint8_t               beep_times; // 离线时蜂鸣次数 (0=静音常亮, >0=闪烁)
    bool                  is_offline; // 当前离线状态
    uint32_t              last_time;  // 上次心跳更新时间
    uint8_t               enable;     // 是否启用检测 (OFFLINE_ENABLE/DISABLE)
    struct OfflineDevice *next;       // 链表下一节点指针
} OfflineDevice;
```

#### Offline_Init_config_t - 离线设备初始化配置 
```c
typedef struct {
    const char *name;       // 设备名称
    uint32_t    timeout_ms; // 超时时间
    uint8_t     beep_times; // 蜂鸣次数 (0表示仅LED常亮)
    uint8_t     enable;     // 初始是否启用
} Offline_Init_config_t;
```

#### OfflineManager_t - 离线管理器 
```c
typedef struct {
    OfflineDevice *device_list;  // 设备链表头指针
    uint8_t        device_count; // 设备数量
    bool           initialized;  // 初始化状态
    volatile bool   silent_error; // 静音故障标志 (针对beep_times=0)
} OfflineManager_t;
```

### 2.2 状态与宏定义 
| 宏/状态 | 值 | 说明 |
|---------|---|------|
| STATE_ONLINE | 0 | 设备在线 |
| STATE_OFFLINE | 1 | 设备离线 |
| OFFLINE_ENABLE | 1 | 启用检测 |
| OFFLINE_DISABLE | 0 | 禁用检测 |
| VALID_CKECK | 0xABCDEF01 | 内存有效声明 |

### 2.3 代码架构 
```c
内部辅助函数层：
└─ beep_ctrl_times()          // Timer回调，控制LED/Beep状态机

公开API层：
├─ Module_Offline_init()      // 模块初始化
├─ Module_Offline_device_register()   // 注册设备
├─ Module_Offline_device_unregister() // 注销设备
├─ Module_Offline_task_function()     // 主任务 (检测与决策)
├─ Module_Offline_device_update()     // 更新心跳
├─ Module_Offline_device_enable()     // 开启检测
├─ Module_Offline_device_disable()    // 关闭检测
└─ Module_Offline_get_device_status() // 获取状态
```
 
## 3. 报警优先级逻辑 
模块内置优先级决策机制，当多个设备同时离线时，按以下优先级显示:
- 高优先级 (闪烁模式)：
    - 如果存在任意 beep_times > 0 的设备离线。
    - 现象：红灯闪烁，蜂鸣器响（次数取所有故障设备中的最小值）。
          
- 低优先级 (常亮模式)：
    - 如果没有高优先级故障，但存在 beep_times == 0 的设备离线。
    - 现象：红灯常亮，蜂鸣器静音。
          
- 正常状态：
    - 所有设备均在线。
    - 现象：绿灯，蜂鸣器静音。
          
## 4. API接口说明 
```c
void Module_Offline_init(void);
```
```c 
OfflineDevice *Module_Offline_device_register(const Offline_Init_config_t *init);
```
```c
void Module_Offline_device_unregister(OfflineDevice *dev);
```
```c
void Module_Offline_task_function(void);
```
```c
void Module_Offline_device_update(OfflineDevice *dev);
```
## 5. 注意事项 

- 周期调用：必须确保 Module_Offline_task_function() 被周期性调用，否则无法触发报警或超时检测。 
- 内存管理：设备节点使用 BSP_MEM_ALLOC_WAIT 分配内存，注销时自动释放。请确保注册后妥善保存句柄指针。 
- 依赖模块：本模块强依赖 BSP_DWT (获取时间戳)、Module_Beep (蜂鸣控制) 和 Module_Rgb (灯效控制)。 

## 6. 常见问题排查 
- 问题：设备一直处于离线状态 
    - 排查：检查通信回调函数中是否正确调用了 update 函数，并检查 DWT 计时器是否正常,task_function是否周期调用

## 7. 使用示例 
```c
// 1. 定义设备配置
static OfflineDevice *g_motor_dev = NULL;

void App_Offline_Init(void)
{
    // 初始化离线模块
    Module_Offline_init();

    // 注册电机设备：超时500ms，报警策略为响1声 (优先闪烁)
    Offline_Init_config_t motor_cfg = {
        .name = "Motor_Left",
        .timeout_ms = 500,
        .beep_times = 1, 
        .enable = OFFLINE_ENABLE
    };
    g_motor_dev = Module_Offline_device_register(&motor_cfg);
    
    // 注册传感器设备：超时1000ms，报警策略为静音常亮 (优先级低于闪烁)
    Offline_Init_config_t sensor_cfg = {
        .name = "Gyro",
        .timeout_ms = 1000,
        .beep_times = 0, 
        .enable = OFFLINE_ENABLE
    };
    Module_Offline_device_register(&sensor_cfg);
}

// 2. 在CAN接收回调中
void Can_Rx_Callback(uint8_t *data)
{
    if (is_motor_data(data)) {
        // 收到电机数据，立即更新
        Module_Offline_device_update(g_motor_dev);
    }
}

// 3. 在任务中周期调用
void offline_task(void)
{
    while(1) {
        // 周期性检测离线状态，建议10ms一次
        Module_Offline_task_function();
        
        tx_thread_sleep(2);
    }
}
```
## 8. 版本历史 
### v1.0 (2026-01-19) 
    - 初始版本

     