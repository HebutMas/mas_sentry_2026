# BSP_CAN 驱动文档

## 1. 概述 
STM32H7 和 STM32F4 系列通用的 CAN 外设管理驱动。统一标准CAN。

## 2. 架构设计

### 2.1 数据结构 

#### Can_Device - CAN 设备实例
```c
typedef struct Can_Device {
    uint32_t            valid;            // 设备有效性检测 (0xABCDEF01)
    #if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan;            // H7: FDCAN句柄
    #elif defined(STM32F407xx)
    CAN_HandleTypeDef   *hcan;            // F4: CAN句柄
    #endif
    uint32_t            tx_id;            // 发送 ID
    uint32_t            rx_id;            // 接收 ID
    uint8_t             rx_buf[8];        // 接收缓冲区 
    uint8_t             real_rx_len;      // 实际接收长度
    uint32_t            eventflag;        // 对应的事件标志
    uint8_t             filter_bank_index;// 占用的硬件过滤器编号
    struct Can_Device  *next;             // 链表下一节点
} Can_Device;
```
#### Can_Device_Init_Config_s - 初始化配置
```c
typedef struct {
    #if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan;
    #elif defined(STM32F407xx)
    CAN_HandleTypeDef   *hcan;
    #endif
    uint32_t tx_id;
    uint32_t rx_id;
} Can_Device_Init_Config_s;
```
### CAN_Bus_Manager - 总线管理器
```c
typedef struct
{
#if defined(STM32H723xx)
    FDCAN_HandleTypeDef *hcan; // CAN句柄
#elif defined(STM32F407xx)
    CAN_HandleTypeDef *hcan; // CAN句柄
#endif
    Can_Device *device_list; // 设备链表头指针
} CAN_Bus_Manager;
```
### 2.2 事件定义

驱动内部自动分配 32 个事件标志位给不同的设备实例。 

| 事件标志 | 获取方式 | 说明 |
|--------- |-------------------|------|
|1 << n	   | device->eventflag |第 n 个分配到的设备的数据接收完成标志|

### 2.3 代码架构
```c
内部辅助函数层：
├─ BSP_CAN_Get_Bus_Manager()      // 获取总线管理器
├─ FDCANConfigFilter() [H7]       // 配置 FDCAN 过滤器
├─ CANConfigFilter() [F4]         // 配置 bxCAN 过滤器
├─ check_device_id_conflict()     // 检查 ID 冲突
└─ BSP_CAN_Get_Event_Flag()      // 分配事件标志位

公开API层：
├─ Can_Event_init()               // can全局事件初始化
├─ BSP_CAN_Device_Init()          // 设备初始化 
├─ BSP_CAN_SendMessage()         // 发送数据
├─ BSP_CAN_ReadSingleDevice()    // 等待单个设备数据
├─ BSP_CAN_ReadMultipleDevice()  // 等待多个设备数据
└─ BSP_CAN_Device_DeInit()       // 销毁设备

HAL回调函数：
├─ HAL_FDCAN_RxFifo0Callback() [H7]
├─ HAL_FDCAN_RxFifo1Callback() [H7]
├─ HAL_CAN_RxFifo1MsgPendingCallback() [F4]
└─ HAL_CAN_RxFifo0MsgPendingCallback() [F4]
```
## 3. 核心机制 

### 3.1 过滤器 
- 匹配 ID 的过滤器策略：
    - F4 (bxCAN): 使用 16 位掩码模式，Mask = 0x7FF，ID = target_rx_id。
    - H7 (FDCAN): 使用掩码模式，Mask = 0x7FF，ID1 = target_rx_id。
- FIFO 分流策略：
    - 偶数 ID -> FIFO 0
    - 奇数 ID  -> FIFO 1

### 4. API 接口说明 
```c
void Can_Event_init(void);
```
```c
Can_Device *BSP_CAN_Device_Init(Can_Device_Init_Config_s *config);
```
```c
UINT BSP_CAN_Device_DeInit(Can_Device *dev);
```
```c
UINT BSP_CAN_SendDevice(Can_Device *device, uint8_t *data, uint8_t len);
```
```c
UINT BSP_CAN_SendMessage(CanTxMessage_t* cantxmeaasge);
```
```c
UINT BSP_CAN_ReadSingleDevice(Can_Device *device, uint32_t timeout);
```
```c
uint32_t BSP_CAN_ReadMultipleDevice(uint32_t eventflags, uint32_t timeout);
```
## 5. 注意事项 

### 5.1 STM32H7 特定注意事项 
    - 缓存一致性: 
        发送：如果数据指针指向 AXI SRAM (SRAM1/2/3)，驱动会自动执行 DCache Clean。
        接收：candevice 声明在dtcmram，ram_d2,ram_d3时不用考虑缓存同步，在ram_d1时需手动同步缓存
## 6. 使用示例 
```c
// 定义电机设备指针
Can_Device *motor1;
CAN_Device_Init_Config_s config;

void app_init(void)
{
    // 初始化 CAN 电机 1
    config.hcan = &hfdcan1;
    config.tx_id = 0x201;
    config.rx_id = 0x201;
    motor1 = BSP_CAN_Device_Init(&config);
    motor2 = BSP_CAN_Device_Init(&config);
}

void motor_control_thread(void *argument)
{
    uint8_t tx_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    while (1)
    {
        tx_data[0] = ...;
        BSP_CAN_SendMessage(motor1, tx_data, 8);
        if (BSP_CAN_ReadSingleDevice(motor1, 10) == TX_SUCCESS)
        {
            // 数据已在 motor1->rx_buf 中，直接解析
            process_feedback(motor1->rx_buf);
        }
        tx_thread_sleep(1);
    }
}
```
## 7. 版本历史

### v1.0 (2026-01-18)
- 初始版本

### v1.1 (2026-01-18)
- 修复f4 can问题

### v1.2 (2026-01-29)
- 移除总线互斥锁
- 移除send函数timeout参数