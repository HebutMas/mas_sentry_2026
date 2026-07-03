# 算法层LQR 模块文档

## 1. 概述
STM32H7 和 STM32F4 系列通用的LQR算法处理模块

## 2. 架构设计

### 2.1 数据结构

#### LQRErrorType -LQR错误类型枚举
```c
// LQR错误类型枚举
typedef enum
{
    LQR_ERROR_NONE = 0,
    LQR_MOTOR_BLOCKED_ERROR
} LQRErrorType;
```

#### LQR_Init_Config_s -LQR初始化配置
```c
typedef struct
{
    float           K[2];             // LQR增益矩阵 K
    uint8_t         state_dim;        // 状态维度
    float           output_max;       // 输出最大值
    float           output_min;       // 输出最小值
} LQR_Init_Config_s;
```
#### LQRInstance -LQR设备实例
```c
typedef struct
{
    float           K[2];      // LQR增益矩阵 K
    uint8_t         state_dim; // 状态维度
    float           err;
    float           output;
    float           state[2];
    float           output_max;       // 输出最大值
    float           output_min;       // 输出最小值

    // 错误处理相关
    struct
    {
        uint32_t     ERRORCount; // 错误计数
        LQRErrorType ERRORType;  // 错误类型
    } ErrorHandler;

    // 输入输出值保存
    float Ref;     // 参考值
    float Measure; // 测量值
} LQRInstance;
```

### 2.2 代码框架
```c
内部辅助函数层：
├─ LQR_ErrorHandle()                      //LQR
└─ calculateOutput()                    // 处理接收完成

公开API层：
├─ LQRInit()          // 设备初始化
└─ LQRCalculate()     // LQR输出处理

```
## 3.控制原理
驱动根据配置的 state_dim 自动切换控制算法：单状态模式 ($n=1$): 主要用于角速度恒定控制。
u = -K_1*(velocity - ref)
双状态模式 ($n=2$): 用于角度位置控制，引入角速度作为阻尼。
u = -K_1*(angle - ref) - K_2*velocity
## 4. API接口说明
```c
void LQRInit(LQRInstance *lqr, LQR_Init_Config_s *config);
```
```c
float LQRCalculate(LQRInstance *lqr, float rad_degree, float rad_angular_velocity, float rad_ref);
```
## 5.注意事项
1. 弧度制优先：建议 rad_degree 使用弧度（Rad），rad_angular_velocity 使用弧度/秒（Rad/s）。
2. 根据控制目标正确配置维度：
设置为 1：仅控制角速度。此时 rad_ref 被视为目标速度。
设置为 2：控制角度位置。此时 rad_ref 被视为目标角度。

## 6.使用示例
```c
// 1. 定义实例与配置
void motor_init()
{
    LQRInstance pitch_lqr;
    LQR_Init_Config_s lqr_config = {
        .state_dim = 2,           // 角度位置控制
        .K = {15.5f, 0.45f},      // 经过仿真或实调得到的 K 矩阵
        .output_max = 5000.0f,
        .output_min = -5000.0f
    };

    // 2. 初始化
    LQRInit(&pitch_lqr, &lqr_config);
}

// 3. 计算输出

float CacalculateLQROutput(void)
{
    float output= LQRCalculate(&pitch_lqr, measure_angle, measure_speed, target);
    //进行后续计算
    return output;
}


```

## 7. 版本历史 

### v1.0 (2026-02-06) 
    - 初始版本
