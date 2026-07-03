# BSP_PWM 驱动文档

## 1. 概述

STM32H7/F4系列MCU通用的PWM外设管理。驱动支持多种工作模式（阻塞/中断/DMA），支持动态占空比调节，采用链表管理多个PWM设备实例，基于ThreadX事件标志组实现异步操作同步。

## 2. 架构设计

### 2.1 数据结构

#### PWM_Device - PWM设备实例
```c
typedef struct PWM_Device {
    TIM_HandleTypeDef   *htim;      // 定时器句柄
    uint32_t             Channel;   // PWM通道
    uint32_t             Period;    // 自动重装载值 (周期，从CubeMX配置读取)
    uint32_t             dutyx10;   // 占空比 (0-1000，对应0.0%-100.0%)
    PWM_Mode             Mode;      // 工作模式
    TX_EVENT_FLAGS_GROUP pwm_event; // PWM事件标志组
    struct PWM_Device   *next;      // 指向下一个PWM设备的指针
} PWM_Device;
```

#### PWM_Init_Config - PWM初始化配置
```c
typedef struct {
    TIM_HandleTypeDef *htim;    // 定时器句柄
    uint32_t           channel; // PWM通道
    uint32_t           dutyx10; // 占空比 (0-1000)
    PWM_Mode           Mode;    // 工作模式
} PWM_Init_Config;
```

#### PWM_Mode - 工作模式枚举
```c
typedef enum {
    PWM_MODE_BLOCKING,  // 阻塞模式
    PWM_MODE_IT,       // 中断模式
    PWM_MODE_DMA       // DMA模式
} PWM_Mode;
```

### 2.2 事件定义

| 事件标志 | 值 | 说明 |
|---------|---|------|
| PWM_EVENT_DONE | 0x01 | PWM完成事件 |
| PWM_EVENT_ERROR | 0x02 | PWM错误事件 |

### 2.3 代码架构

```
内部辅助函数层：
├─ PWM_Find_Device()              // 根据句柄和通道查找设备
├─ PWM_Convert_Dutyx10_To_Pulse() // 占空比转换为脉冲宽度
└─ HAL_TIM_PWM_PulseFinishedCallback() // PWM脉冲完成回调
└─ HAL_TIM_ErrorCallback()        // 错误处理回调

公开API层：
├─ BSP_PWM_Device_Init()          // 设备初始化
├─ BSP_PWM_Device_DeInit()        // 设备去初始化
├─ BSP_PWM_Start()                // 启动PWM输出
├─ BSP_PWM_Stop()                 // 停止PWM输出
├─ BSP_PWM_SetDutyCycle()         // 设置占空比
├─ BSP_PWM_SetPulse()             // 设置脉冲宽度
└─ BSP_PWM_WaitForComplete()      // 等待完成事件
```

## 3. API接口说明

```c
PWM_Device *BSP_PWM_Device_Init(PWM_Init_Config *config);
```
```c
UINT BSP_PWM_Device_DeInit(PWM_Device *dev);
```
```c
UINT BSP_PWM_Start(PWM_Device *dev, uint8_t *data, uint8_t len);
```
```c
UINT BSP_PWM_Stop(PWM_Device *dev);
```
```c
UINT BSP_PWM_SetDutyCycle(PWM_Device *dev, uint16_t duty);
```
```c
UINT BSP_PWM_SetPulse(PWM_Device *dev, uint32_t pulse);
```
```c
UINT BSP_PWM_WaitForComplete(PWM_Device *dev, ULONG wait_option);
```

## 4. 注意事项

**重要：** DMA模式下的缓冲区要求：
```c
// DMA缓冲区必须32字节对齐
ALIGN(32) uint32_t pwm_data_buffer[BUFFER_SIZE];
```

**DMA缓存同步（H7系列）：**
- H7系列在DMA模式下会自动同步D-Cache
- 使用SCB_CleanDCache_by_Addr()确保DMA读取最新数据

**占空比范围：**
- dutyx10参数范围：0-1000
- 对应实际占空比：0.0%-100.0%
- 超出范围会被自动限制

**定时器配置：**
- Period值从CubeMX配置中自动读取
- 脉冲宽度不能超过Period值

## 5. 常见问题排查

### 问题1：初始化失败，返回NULL
**可能原因：**
- 定时器句柄为NULL
- 相同定时器和通道的设备已存在
- ThreadX事件标志组创建失败
- 内存分配失败

### 问题2：PWM启动失败
**可能原因：**
- 设备指针或定时器句柄无效
- DMA模式下未提供有效的数据缓冲区
- HAL库返回错误状态

### 问题3：占空比设置无效
**可能原因：**
- 占空比值超出范围（会被自动限制）
- 定时器未正确初始化
- 通道配置错误

### 问题4：DMA模式数据不正确
**可能原因：**
- 缓冲区未32字节对齐
- 缓冲区在DMA无法访问的内存区域（H7）

## 6. 使用示例
```c
// DMA缓冲区（必须32字节对齐）
ALIGN(32) uint32_t pwm_wave_data[100];

// 初始化配置
PWM_Init_Config config = {
    .htim = &htim4,
    .channel = TIM_CHANNEL_3,
    .dutyx10 = 300,  // 30.0%占空比
    .Mode = PWM_MODE_DMA
};

PWM_Device *pwm4_ch3 = BSP_PWM_Device_Init(&config);

// 准备PWM波形数据
for (int i = 0; i < 100; i++) {
    pwm_wave_data[i] = (i * pwm4_ch3->Period) / 100;  // 线性变化
}

// 启动DMA模式PWM
BSP_PWM_Start(pwm4_ch3, (uint8_t*)pwm_wave_data, 100);

// 等待完成
if (BSP_PWM_WaitForComplete(pwm4_ch3, TX_WAIT_FOREVER) == TX_SUCCESS) {
    log_i("DMA PWM transmission completed");
}
```

## 7. 版本历史

### v1.0 (2026-01-14)
- 初始版本

### v1.1 (2026-01-26)
- 增加PSC分频数设置函数
- 合并h7/f4

