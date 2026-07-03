# BSP_I2C 驱动文档

## 1. 概述

STM32F4系列MCU通用的I2C外设管理。驱动支持多种传输模式（阻塞/中断/DMA）。

## 2. 架构设计

### 2.1 数据结构

#### I2C_Device - I2CI设备实例
```c
typedef struct I2C_Device
{
    uint32_t           valid;       // 设备指针有效性检测标志
    I2C_HandleTypeDef *hi2c;        // I2C句柄
    uint16_t           dev_address; // 设备地址,注意在stm32中是左移1位的地址
    I2C_Mode           tx_mode;     // 发送模式
    I2C_Mode           rx_mode;     // 接收模式
    struct I2C_Device *next;        // 链表下一个节点指针
} I2C_Device;
```

#### I2C_Bus_Manager - I2C总线管理器
```c
/* I2C总线管理结构 */
typedef struct
{
    I2C_HandleTypeDef   *hi2c;        // I2C句柄
    I2C_Device          *device_list; // 设备链表头指针
    TX_EVENT_FLAGS_GROUP bus_event;   // 总线事件
    TX_SEMAPHORE         bus_sem;     // 总线信号量(用于保护总线，防止多线程竞争)
} I2C_Bus_Manager;
```

#### I2C_Mode - 传输模式枚举
```c
typedef enum
{
    I2C_MODE_BLOCKING,
    I2C_MODE_IT,
    I2C_MODE_DMA
} I2C_Mode;
```

### 2.2 事件定义

| 事件标志 | 值 | 说明 |
|---------|---|------|
| I2C_EVENT_TX_COMPLETE | 0x01 | 发送完成事件 |
| I2C_EVENT_RX_COMPLETE | 0x02 | 接收完成事件 |
| I2C_EVENT_ERROR | 0x04 | 错误事件 |

### 2.3 代码架构

```
内部辅助函数层：
├─ BSP_I2C_Get_Bus_Manager()       // 获取总线管理器
├─ BSP_I2C_Pre_Transfer_Init()     // 传输前置检查和初始化
└─ BSP_I2C_Common_Transfer()       // 通用传输核心逻辑

公开API层：
├─ BSP_I2C_Device_Init()          // 设备初始化
├─ BSP_I2C_Device_DeInit()        // 设备去初始化
├─ BSP_I2C_Transmit()            // 发送
├─ BSP_I2C_Receive()             // 接收
├─ BSP_I2C_Mem_Write()           // 带内存地址发送
└─ BSP_I2C_Mem_Read()            // 带内存地址接收

HAL回调函数：
├─ HAL_I2C_MasterTxCpltCallback()
├─ HAL_I2C_MasterRxCpltCallback()
├─ HAL_I2C_MemTxCpltCallback()
├─ HAL_I2C_MemRxCpltCallback()
└─ HAL_I2C_ErrorCallback()
```

## 3. API接口说明

```c
I2C_Device *BSP_I2C_Device_Init(I2C_Device_Init_Config *config);
```
```c
UINT BSP_I2C_Device_DeInit(I2C_Device *dev);
```
```c
UINT BSP_I2C_Transmit(I2C_Device *dev, uint8_t *tx_data, uint16_t size, uint32_t timeout);
```
```c
UINT BSP_I2C_Receive(I2C_Device *dev, uint8_t *rx_data, uint16_t size, uint32_t timeout);
```
```c
UINT BSP_I2C_Mem_Write(I2C_Device *dev, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data,
                       uint16_t size, uint32_t timeout);
```
```c
UINT BSP_I2C_Mem_Read(I2C_Device *dev, uint16_t mem_address, uint16_t mem_add_size, uint8_t *data,
                      uint16_t size, uint32_t timeout);
```

## 4. 注意事项
**重要**：目前只实现了f4的，h7暂未添加，后续根据需要添加

### 5. 错误处理
所有传输函数返回状态码：
- `TX_SUCCESS`: 操作成功
- `TX_DELETED`: 操作失败（参数错误、设备无效、总线忙等）

### 6. 常见问题排查

**问题1：传输总是失败**
- 检查设备初始化是否成功
- 检查I2C速率配置是否匹配

**问题2：DMA传输错误**
- 检查缓冲区是否在DMA可访问区域
- 检查缓冲区是否正确对齐
- 查看日志中的DMA错误详情

## 7. 版本历史

### v1.0 (2026-01-15)
- 初始版本

### v1.1 (2026-01-16)
- 总线保护修改为信号量
- 增加系统初始化环境判断
- 系统初始化时(tx_application_define)强制使用blocking函数

### v1.2 (2026-01-17)
- 总线保护更换为互斥锁
