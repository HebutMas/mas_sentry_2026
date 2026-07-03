# BSP_UART 驱动文档

## 1. 概述

STM32H7/F4系列MCU通用的UART外设管理。驱动支持多种传输模式（阻塞/中断/DMA），支持定长和不定长接收，采用双缓冲机制提高接收效率。

## 2. 架构设计

### 2.1 数据结构

#### UART_Device - UART设备实例
```c
typedef struct UART_Device {
    uint32_t            valid;            // 设备有效性检测 
    UART_HandleTypeDef *huart;            // UART硬件句柄
    uint8_t (*rx_buf)[2];                 // 双缓冲区指针，32字节对齐
    uint16_t             rx_buf_size;     // 缓冲区大小
    volatile uint8_t     rx_active_buf;   // 当前活动缓冲区索引
    uint16_t             real_rx_len;     // 实际接收数据长度
    uint16_t             expected_rx_len; // 预期长度（0为不定长）
    TX_EVENT_FLAGS_GROUP uart_event;      // UART事件标志组
    UART_Mode            rx_mode;         // 接收模式
    UART_Mode            tx_mode;         // 发送模式
    struct UART_Device  *next;            // 链表下一节点
} UART_Device;
```

#### UART_Device_init_config - UART初始化配置
```c
typedef struct {
    UART_HandleTypeDef *huart;       // UART硬件句柄
    uint8_t (*rx_buf)[2];           // 外部接收缓冲区指针，32字节对齐
    uint16_t  rx_buf_size;          // 接收缓冲区大小
    uint16_t  expected_rx_len;      // 预期长度（0为不定长）
    UART_Mode rx_mode;              // 接收模式
    UART_Mode tx_mode;              // 发送模式
} UART_Device_init_config;
```

#### UART_Mode - 传输模式枚举
```c
typedef enum {
    UART_MODE_BLOCKING,  // 阻塞模式
    UART_MODE_IT,       // 中断模式
    UART_MODE_DMA       // DMA模式（H7支持缓存同步）
} UART_Mode;
```

### 2.2 事件定义

| 事件标志 | 值 | 说明 |
|---------|---|------|
| UART_RX_DONE_EVENT | 0x01 | 接收完成事件 |
| UART_TX_DONE_EVENT | 0x02 | 发送完成事件 |
| UART_ERROR_EVENT | 0x04 | 错误事件 |

### 2.3 代码架构

```
内部辅助函数层：
├─ Start_Rx()                      // 启动UART接收
├─ Process_Rx_Complete()          // 处理接收完成
└─ BSP_UART_Find_Device()         // 根据句柄查找设备

公开API层：
├─ BSP_UART_Device_Init()          // 设备初始化
├─ BSP_UART_Send()                // 发送数据
├─ BSP_UART_Read()                // 读取数据
└─ BSP_UART_Deinit()              // 设备去初始化

HAL回调函数：
├─ HAL_UARTEx_RxEventCallback()   // 接收事件回调
├─ HAL_UART_TxCpltCallback()      // 发送完成回调
└─ HAL_UART_ErrorCallback()       // 错误处理回调
```

## 3. API接口说明

```c
UART_Device *BSP_UART_Device_Init(UART_Device_init_config *config);
```
```c
int BSP_UART_Send(UART_Device *device, uint8_t *data, uint16_t len, uint32_t timeout);
```
```c
uint8_t *BSP_UART_Read(UART_Device *device, uint32_t timeout);
```
```c
UINT BSP_UART_Deinit(UART_Device *device);
```
## 4. 双缓冲机制

UART驱动采用双缓冲机制，在非阻塞模式下实现乒乓接收：

```
接收流程：
缓冲区A (非活动)    ←  HAL回调完成   ←  缓冲区B (活动)
     ↑                                       ↓
     |                                       |
   用户读取                              DMA/IT接收中
     ↓                                       ↑
缓冲区B (非活动)    ←  HAL回调完成   ←  缓冲区A (活动)
```

## 5. 注意事项

**重要：** 所有DMA缓冲区必须满足下面要求：
```c
   ALIGN(32) uint8_t uart_rx_buf[2][RX_BUF_SIZE];  // 双缓冲数组且32字节对齐
```
**DMA不可访问内存区域（H7）**：DMA无法访问以下内存区域
   - DTCMRAM: 0x20000000 - 0x2001FFFF (128KB)
   - ITCMRAM: 0x00000000 - 0x0000FFFF (64KB)
> H7系列在DMA模式下会自动同步缓存

## 6. 常见问题排查

### 问题1：初始化失败，返回NULL
**可能原因：**
- 缓冲区地址未32字节对齐
- 缓冲区不是双缓冲结构
- 缓冲区在DMA无法访问的内存区域（H7）
- UART实例已存在，重复初始化

### 问题2：接收数据不正确
**可能原因：**
- 波特率配置不匹配
- 缓冲区未及时拷贝，数据被覆盖
- 定长/不定长配置错误

## 7. 使用示例（DMA）
```c
BUFFER_SECTION __attribute__((aligned(32))) uint8_t uart_rx_buf[2][256];
BUFFER_SECTION uint8_t uart_tx_buf[24];

UART_Device_init_config config = {
    .huart = &huart1,
    .rx_buf = (uint8_t (*)[2])uart_rx_buf,
    .rx_buf_size = 256,
    .expected_rx_len = 0,  // 不定长接收
    .rx_mode = UART_MODE_DMA,
    .tx_mode = UART_MODE_DMA
};

UART_Device *uart2 = BSP_UART_Device_Init(&config);

// 在线程中接收或发送
while (1) {

    BSP_UART_Send(uart2,uart_tx_buf,sizeof(uart_tx_buf),TX_WAIT_FOREVER);

    uint8_t *data = BSP_UART_Read(uart2,TX_WAIT_FOREVER);
    if (data != NULL) {
        // 处理数据
        parse_data(data, uart2->real_rx_len);
    }
}
```

## 8. 版本历史

### v1.0 (2026-01-14)
- 初始版本

### v1.1 (2026-01-15)
- 增加超时参数
- 增加h7的dma发送时内存地址检查
- 更新示例
