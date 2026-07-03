# MODULE_MOTOR_DAMIAO 达妙电机驱动模块文档

## 1. 概述

该模块适用于达妙 (Damiao) 全系列动力电机（如 DM4310, DM6220 等）。模块支持达妙特有的 MIT、位置速度、速度、以及 PSI（位置速度电流）四种控制模式，并集成了多模式切换、自动掉线检测、以及 PID/LQR 闭环算法支持。

---

## 2. 架构设计

### 2.1 数据结构

#### DMMotorError_t - 电机错误类型枚举
```c
typedef enum
{
    DM_NO_ERROR               = 0X00,
    OVERVOLTAGE_ERROR         = 0X08,
    UNDERVOLTAGE_ERROR        = 0X09,
    OVERCURRENT_ERROR         = 0X0A,
    MOS_OVERTEMP_ERROR        = 0X0B,
    MOTOR_COIL_OVERTEMP_ERROR = 0X0C,
    COMMUNICATION_LOST_ERROR  = 0X0D,
    OVERLOAD_ERROR            = 0X0E,
} DMMotorError_t;
```

#### DMMotor_Mode_e - 电机控制指令枚举
```c
typedef enum
{
    DM_CMD_MOTOR_START   = 0xfc, // 使能,会响应指令
    DM_CMD_MOTOR_STOP    = 0xfd, // 停止
    DM_CMD_ZERO_POSITION = 0xfe, // 将当前的位置设置为编码器零位
    DM_CMD_CLEAR_ERROR   = 0xfb  // 清除电机错误
} DMMotor_Mode_e;
```

#### DM_Motor_Measure_s - 电机测量反馈

存储从 CAN 帧解析后的物理量，达妙电机反馈采用浮点数映射（uint to float）。

```c
typedef struct {
    uint8_t        id;                      // 反馈 ID
    float          velocity;                // 角速度 (rad/s)
    float          angle_single_round;      // 单圈角度 (rad)
    float          torque;                  // 当前力矩 (Nm)
    float          T_Mos;                   // MOS 管温度
    float          T_Rotor;                 // 线圈温度
    float          total_angle;             // 累计弧度 (rad)
    int32_t        total_round;             // 累计圈数
    DMMotorError_t Error_Code;              // 错误状态码
} DM_Motor_Measure_s;

```

#### DM_MOTOR_t - 电机实例

```c
typedef struct DM_MOTOR_t {
    DM_Motor_Measure_s measure;           // 测量值
    Motor_Info_s       motor_info;        // 硬件参数 (max_torque 等)
    Motor_Controller_s motor_controller;  // PID/LQR 控制器
    Motor_Setting_s    motor_setting;     // 闭环/使能设置
    uint32_t           DMMotor_Mode_type; // 控制模式偏移量 (MIT/POS/SPD/PSI)
    OfflineDevice     *offline_dev;       // 掉线检测句柄
    Can_Device        *can_dev;           // CAN 设备引用
    struct DM_MOTOR_t *next;              // 链表指针
} DM_MOTOR_t;

```

### 2.2 控制模式与 ID 偏移

达妙电机通过控制帧 ID 的偏移来识别不同的控制模式：

| 模式宏 | 偏移值 | 说明 |
| --- | --- | --- |
| `DM_MIT_MODE` | 0x000 | 纯 MIT 协议（位置、速度、Kp、Kd、力矩） |
| `DM_POS_MODE` | 0x100 | 位置速度模式（4字节位置 + 4字节速度） |
| `DM_SPD_MODE` | 0x200 | 纯速度模式（4字节速度） |
| `DM_PSI_MODE` | 0x300 | 位置/速度/电流综合模式 |

### 2.3 代码架构
```c
内部辅助函数层：
├─ mit_ctrl()          //mit控制
├─ pos_speed_ctrl()    //位置速度双环控制
├─ speed_ctrl()        //速度环控制   
├─ psi_ctrl()          //位置环控制 
├─ CalculatePIDOutput()//调用下层pid计算输出 
└─ CalculateLQROutput()//调用下层lqr计算输出

公开API层：
├─ Motor_DM_Cmd()      //达妙电机发送控制命令
├─ Motor_DM_Init()   // 达妙电机初始化
├─ Motor_DM_Start() // 达妙电机使能/启动
├─ Motor_DM_Stop()     // 达妙电机停止
├─ Motor_DM_ChangeFeed()     //达妙电机修改对应闭环的反馈数据源
├─ Motor_DM_OuterLoop()     // 达妙电机设置外环控制类型
├─ Motor_DM_SetRef()    // 达妙电机设置参考值
├─ Motor_DM_Control()  // 达妙电机控制函数，在电机线程中周期调用
└─ Motor_DM_Decode() //大喵电机数据解析，处理CAN接收到的电机数据
```
---

## 3. 核心逻辑

### 3.1 数据映射 (Data Mapping)

达妙电机在 MIT 模式下使用 12/16 位无符号整数表示浮点物理量。

* **发送**：使用 `float_to_uint` 将 Nm 或 rad/s 转换为二进制段。
* **接收**：使用 `uint_to_float` 将 12/16 位数据还原为物理量。

### 3.2 保护机制

* **初始化保护**：模块初始化时会自动发送 `DM_CMD_CLEAR_ERROR` 和 `DM_CMD_MOTOR_START`，确保电机处于受控状态。
* **离线安全**：当 `Module_Offline` 检测到电机离线时，`Motor_DM_Control` 会强制将控制器输出归零，并停止发送控制帧（或发送零力矩帧）。

---

## 4. API 接口说明


```c
DM_MOTOR_t *Motor_DM_Init(Motor_Init_Config_s *config, uint32_t DM_Mode_type);
void Motor_DM_Cmd(DM_MOTOR_t *motor, DMMotor_Mode_e cmd);
```
```c
void Motor_DM_Start(DM_MOTOR_t *motor);
```
```c
void Motor_DM_Stop(DM_MOTOR_t *motor);
```
```c
void Motor_DM_ChangeFeed(DM_MOTOR_t *motor, Closeloop_Type_e loop, uint8_t feedback_source);
```
```c
void Motor_DM_OuterLoop(DM_MOTOR_t *motor, Closeloop_Type_e closeloop_type);
```
```c
void Motor_DM_SetRef(DM_MOTOR_t *motor, float ref);
```
```c
void Motor_DM_Decode(DM_MOTOR_t *motor);
```
```c
void Motor_DM_Control(void);
```
---

## 5. 注意事项

- 发送频率：官方建议 1 路 CAN 控制 3 个电机时频率为 1kHz；6 个电机建议降至 0.5kHz 且每帧间隔 200us。
- MIT 范围限制：参数必须严格限制在 `DM_P_MIN/MAX` 等宏定义的量程内，否则映射会溢出。
- 模块依赖：本模块依赖BSP_CAN(通信)，Moudule_Offline(掉线检测)
---

## 6. 常见问题排查

* 电机无法启动：
1. 检查 `Error_Code` 是否为 `DM_NO_ERROR`。
2. 是否start电机
3. 电机是否掉线
---

## 7. 使用示例

```c
static DM_MOTOR_t  *motor     = NULL; // pitch电机指针

// 1. 定义配置
void App_gimbal_Init()
{
    Motor_Init_Config_s motor_config = {
        .can_device_init_config =
            {
                .hcan  = &hcan1,
                .tx_id = 0X23,
                .rx_id = 0X206,
            },
        .controller_init_config =
            {
                .other_angle_feedback_ptr =&angle_feedback_source,
                .other_speed_feedback_ptr =&speed_feedback_source
                    &bmi088_dev->gyro[0],
                .lqr_init =
                    {
                        ...
                    },
            },
        .setting_init_config =
            {
                .algorithm_type        = CONTROL_LQR,
                .feedback_reverse_flag = 1,
                .angle_feedback_source = 1,
                .speed_feedback_source = 1,
                .loop_type             = ANGLE_LOOP,
            },
        .Motor_init_Info = {
            .motor_type = DM4310, .gear_ratio = 10, .max_torque = 7, .torque_constant = 0.093f}};
// 2. 初始化为 MIT 模式
    motor = Motor_DM_Init(&motor_config, DM_MIT_MODE);

}    

// 3.在云台线程中设置参数

void gimbal_task(void)
{
    while(1)
    {
        Motor_DM_Start(motor);
        Motor_DM_SetRef(motor, ref);
        tx_thread_sleep(2);

    }
}

// 4. 在电机线程中进行控制

void motor_task(void)
{
    while(1)
    {
        Motor_DM_Control();
        tx_thread_sleep(2);
    }
}
```

---

## 8. 版本历史

### v1.0 (2026-02-18)

- 初始版本。

---