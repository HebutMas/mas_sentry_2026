# BSP_SPI 驱动文档

## 1. 概述

STM32H7/F4系列MCU通用的SPI外设管理。驱动支持多种传输模式（阻塞/中断/DMA）。

## 2. 架构设计

### 2.1 数据结构

#### SPI_Device - SPI设备实例
```c
typedef struct SPI_Device {
    uint32_t           valid;   // 设备有效性检测 (0xABCDEF01)
    SPI_HandleTypeDef   *hspi;    // SPI硬件句柄
    GPIO_TypeDef       *cs_port;  // CS引脚端口
    uint16_t           cs_pin;   // CS引脚编号
    SPI_Mode           tx_mode;  // 发送模式 (BLOCKING/IT/DMA)
    SPI_Mode           rx_mode;  // 接收模式 (BLOCKING/IT/DMA)
    struct SPI_Device *next;     // 链表下一节点
} SPI_Device;
```

#### SPI_Bus_Manager - SPI总线管理器
```c
typedef struct SPI_Bus_Manager {
    SPI_HandleTypeDef    *hspi;         // SPI硬件句柄
    SPI_Device          *device_list;    // 设备链表头
    TX_EVENT_FLAGS_GROUP bus_event;     // 总线事件标志组
    TX_SEMAPHORE         bus_sem;     // 总线信号量(用于保护总线，防止多线程竞争)
} SPI_Bus_Manager;
```

#### SPI_Mode - 传输模式枚举
```c
typedef enum {
    SPI_MODE_BLOCKING,  // 阻塞模式
    SPI_MODE_IT,       // 中断模式
    SPI_MODE_DMA        // DMA模式（H7支持缓存同步）
} SPI_Mode;
```

### 2.2 事件定义

| 事件标志 | 值 | 说明 |
|---------|---|------|
| SPI_EVENT_TX_DONE_EVENT | 0x01 | 发送完成事件 |
| SPI_EVENT_RX_DONE_EVENT | 0x02 | 接收完成事件 |
| SPI_EVENT_TX_RX_DONE_EVENT | 0x04 | 收发完成事件 |
| SPI_EVENT_ERROR_EVENT | 0x08 | 错误事件 |

### 2.3 代码架构

```
内部辅助函数层：
├─ BSP_SPI_Get_Bus_Manager()      // 获取总线管理器
├─ BSP_SPI_Check_DMA_Buffer()     // DMA缓冲区检查（H7）
├─ BSP_SPI_Select_Device()        // 选中设备（拉低CS）
├─ BSP_SPI_Deselect_Device()      // 取消选中（拉高CS）
├─ BSP_SPI_Pre_Transfer_Init()    // 传输前置检查和初始化
├─ BSP_SPI_Post_Transfer_Cleanup() // 传输后置清理
└─ BSP_SPI_Common_Transfer()      // 通用传输核心逻辑

公开API层：
├─ BSP_SPI_Device_Init()          // 设备初始化
├─ BSP_SPI_Device_DeInit()        // 设备去初始化
├─ BSP_SPI_Transmit()            // 发送
├─ BSP_SPI_Receive()             // 接收
├─ BSP_SPI_TransReceive()         // 收发
├─ BSP_SPI_TransAndTrans()        // 连续两次发送
└─ BSP_SPI_ReceAndRece()        // 连续两次接收

HAL回调函数：
├─ HAL_SPI_TxCpltCallback()
├─ HAL_SPI_RxCpltCallback()
├─ HAL_SPI_TxRxCpltCallback()
└─ HAL_SPI_ErrorCallback()
```

## 3. API接口说明

```c
SPI_Device *BSP_SPI_Device_Init(SPI_Device_Init_Config *config);
```
```c
UINT BSP_SPI_Device_DeInit(SPI_Device *dev);
```
```c
UINT BSP_SPI_Transmit(SPI_Device *dev, const uint8_t *tx_data, uint16_t size,uint32_t timeout);
```
```c
UINT BSP_SPI_Receive(SPI_Device *dev, uint8_t *rx_data, uint16_t size,uint32_t timeout);
```
```c
UINT BSP_SPI_TransReceive(SPI_Device *dev, const uint8_t *tx_data, uint8_t *rx_data, uint16_t size,uint32_t timeout);
```
```c
UINT BSP_SPI_TransAndTrans(SPI_Device *dev, const uint8_t *tx_data1, uint16_t size1,
                           const uint8_t *tx_data2, uint16_t size2,uint32_t timeout);
```
```c
UINT BSP_SPI_ReceAndRece(SPI_Device *dev, uint8_t *rx_data1, uint16_t size1,
                         uint8_t *rx_data2, uint16_t size2,uint32_t timeout);
```

## 4. 注意事项
**重要**：DMA无法访问以下内存区域，使用这些区域作为DMA缓冲区会导致错误：
- DTCMRAM: 0x20000000 - 0x2001FFFF (128KB)
- ITCMRAM: 0x00000000 - 0x0000FFFF (64KB)
> 其余内存区域，驱动会自动执行同步缓存操作

### 5. 错误处理
所有传输函数返回状态码：
- `TX_SUCCESS`: 操作成功
- `TX_DELETED`: 操作失败（参数错误、设备无效、总线忙等）

### 6. 常见问题排查

**问题1：传输总是失败**
- 检查设备初始化是否成功
- 检查CS引脚配置是否正确
- 检查SPI时钟配置是否匹配
  例如：
    - IT模式报HAL_SPI_ERROR_OVR：spi速率与it中断速度匹配不上,降低spi速率才能正常使用

**问题2：DMA传输错误**
- 检查缓冲区是否在DMA可访问区域
  例如：
    - spi6使用BDMA传输，只有将buffer定义在RAM_D3，才能正常使用
- 检查缓冲区是否正确对齐
- 查看日志中的DMA错误详情

**问题3：读取数据不正确**
- 检查SPI模式配置（CPOL/CPHA）
- 检查时序参数（时钟分频）
- 使用逻辑分析仪验证SPI时序

## 7. 版本历史

### v1.0 (2026-01-14)
- 初始版本

### v1.1 (2026-01-15)
- 增加无cs引脚的spi设备功能

### v1.2 (2026-01-15)
- 重构BSP_SPI_Common_Transfer，修复与hal库传输参数不一致问题
- 增加超时参数

### v1.3 (2026-01-16)
- 移除双链表，改为单链表
- 使用mutex作总线保护
- 无线程环境下，强制使用blocking
