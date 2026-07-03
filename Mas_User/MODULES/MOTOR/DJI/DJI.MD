# MODULE_MOTOR_DJI 大疆电机驱动模块文档

## 1. 概述

该模块适用于大疆 (DJI) 全系列动力电机（如 M3508, M2006, GM6020）。模块通过 CAN 总线进行通信，支持多电机分组发送机制，集成 PID 与 LQR 闭环算法，并具备自动过零处理的多圈角度累加功能。

---

## 2. 架构设计

### 2.1 数据结构

#### DJI_Measure_s - DJI 电机测量反馈

存储从 CAN 帧解析后的原始数据与物理量。

```c
typedef struct
{
    uint16_t last_ecd;           // 上一次读取的编码器值
    uint16_t ecd;                // 0-8191,刻度总共有8192格
    float    angle_single_round; // 单圈角度（rad）
    float    speed_rad;          // (rad/s)
    float    speed_rpm;          // rpm
    int16_t  real_current;       // 实际电流 A
    uint8_t  temperature;        // 温度 Celsius
    float    total_angle;        // 总角度,注意方向(rad)
    int32_t  total_round;        // 总圈数,注意方向
} DJI_Measure_s;

```

#### DJI_Motor_t - DJI 电机实例

```c
typedef struct DJI_Motor_t
{
    Motor_Info_s         motor_info;       // 电机硬件参数
    Motor_Controller_s   motor_controller; // 电机控制器 (PID/LQR)
    Motor_Setting_s      motor_setting;    // 电机设置 (使能、闭环类型)
    DJI_Measure_s       measure;          // 电机测量数据
    uint8_t              sender_group;     // 发送组号 (映射到具体 CAN ID)
    uint8_t              message_num;      // 组内序号 (0-3)
    OfflineDevice       *offline_dev;      // 离线设备指针
    Can_Device          *can_dev;          // CAN 设备句柄
    struct DJI_Motor_t *next;             // 链表下一个节点指针
} DJI_Motor_t;

```

### 2.2 发送分组逻辑

由于大疆协议规定一帧 CAN 报文可携带 4 个电机的控制电流，模块内置了自动分组映射机制：

| 电机类型 | ID 范围 | CAN 发送 ID | 备注 |
| --- | --- | --- | --- |
| M3508/M2006 | 1-4 | 0x200 | 常用 ID 分组 1 |
| M3508/M2006 | 5-8 | 0x1FF | 常用 ID 分组 2 |
| GM6020 | 1-4 | 0x1FE (电压) / 0x1FF (电流) | 根据控制模式切换 |
| GM6020 | 5-7 | 0x2FE (电压) / 0x2FF (电流) | 根据控制模式切换 |

### 2.3 代码架构

```c
内部辅助函数层：
├─ MotorSenderGrouping() // 核心逻辑：自动计算发送ID、接收ID及分组偏移
├─ CalculatePIDOutput()  // 执行串级PID计算并完成电流映射
├─ CalculateLQROutput()  // 执行LQR状态空间计算
└─ currentToInteger()    // 物理电流/力矩值到16位寄存器值的线性映射

公开API层：
├─ Motor_DJI_Init()      // 电机初始化、内存分配、CAN设备注册与分组
├─ Motor_DJI_Start()     // 电机使能
├─ Motor_DJI_Stop()      // 电机紧急停止（输出清零）
├─ Motor_DJI_ChangeFeed() // 修改角度/速度环的反馈来源（电机/外部传感器）
├─ Motor_DJI_OuterLoop() // 动态修改控制外环（位置/速度环切换）
├─ Motor_DJI_SetRef()    // 设置控制目标（弧度或转速）
├─ Motor_DJI_Control()   // 链表遍历，执行控制计算并触发CAN批量发送
└─ Motor_DJI_Decode()    // 数据解析，处理过零跳变与多圈累加

```

---

## 3. 核心逻辑

### 3.1 多圈累加逻辑

通过记录 `last_ecd` 并对比当前 `ecd`，当差值大于半圈（4096）时判定为发生过零跳变：

* `delta_ecd > 4096`：圈数递减。
* `delta_ecd < -4096`：圈数递增。
* 最终角度 = 。

### 3.2 保护与安全

* 离线处理：当 `Module_Offline` 判定状态为 `STATE_OFFLINE` 时，`Motor_DJI_Control` 会强制将电流输出及 PID 积分项清零。
* 输出限幅：针对不同型号（M3508/M2006/GM6020）设置了不同的最大电流映射范围（如 10000 或 16384）。

---

## 4. API 接口说明

```c
DJI_Motor_t *Motor_DJI_Init(Motor_Init_Config_s *config);

```

```c
void Motor_DJI_Start(DJI_Motor_t *motor);

```

```c
void Motor_DJI_Stop(DJI_Motor_t *motor);

```

```c
void Motor_DJI_ChangeFeed(DJI_Motor_t *motor, Closeloop_Type_e loop, uint8_t feedback_source);

```

```c
void Motor_DJI_OuterLoop(DJI_Motor_t *motor, Closeloop_Type_e outer_loop);

```

```c
void Motor_DJI_SetRef(DJI_Motor_t *motor, float ref);

```

```c
void Motor_DJI_Control(void);

```

```c
void Motor_DJI_Decode(DJI_Motor_t *motor);

```

---

## 5. 注意事项

- ID 冲突：初始化时会自动检查同一路 CAN 总线上是否存在相同的接收 ID。如果冲突，初始化将失败并输出 Error Log。
- 硬件平台：模块通过宏定义支持 `STM32H7` (支持 CAN1/2/3) 和 `STM32F4` (仅 CAN1/2) 平台。
- 反馈转换：编码器原始值 (0-8191) 会通过 `ECD_ANGLE_COEF_DJI_RAD` 转换为标准国际单位（弧度）。
- 模块依赖：本模块依赖BSP_CAN(通信)，Moudule_Offline(掉线检测)。

---

## 6. 常见问题排查

- 电机反转：检查 `motor_reverse_flag`（控制反向）与 `feedback_reverse_flag`（反馈反向）的设置。
- 无法收到反馈：检查 CAN 过滤器的 `rx_id` 是否正确映射。
* 电机无法启动：
1. 检查 `Error_Code` 是否为 `DM_NO_ERROR`。
2. 是否start电机
3. 电机是否掉线

---

## 7. 使用示例

```c
static DJI_Motor_t *pitch_motor = NULL;

void App_Motor_Init(void)
{
    Motor_Init_Config_s config = {
        .can_device_init_config = { .hcan = &hfdcan1, .tx_id = 1 }, // 拨码开关ID为1
        .Motor_init_Info = { .motor_type = M3508, .max_torque = 3.0f },
        .setting_init_config = { 
            .algorithm_type = CONTROL_PID,
            .loop_type = ANGLE_LOOP | SPEED_LOOP // 位置速度串级
        }
    };
    motor = Motor_DJI_Init(&config);
    Motor_DJI_Start(motor);
}

void chassis_task(void)
{
    while(1)
    {              
        Motor_DJI_Start(motor);
        Motor_DJI_SetRef(motor,ref);
        tx_thread_sleep(2);

    }
    
}

void Motor_Task(void)
{
    while(1) {
        Motor_DJI_Control();
        tx_thread_sleep(2);
    }
}

```

---

## 8. 版本历史

### v1.0 (2026-02-18)

- 初始版本

---
