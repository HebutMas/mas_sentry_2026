# 模块层 板间通信 (Board Comm) 驱动文档

## 1. 概述

板间通信模块专门用于 RoboMaster 机器人中**云台主控板**与**底盘主控板**之间的数据同步。通过 CAN 总线实现控制指令（速度、模式、角度偏移）的下发以及底盘状态（功率、反馈信息）的上报，并内置了离线监测机制。

---

## 2. 架构设计

### 2.1 拓扑结构

系统根据宏定义 `GIMBAL_BOARD` 或 `CHASSIS_BOARD` 自动切换角色，实现双向对称通信。

### 2.2 数据结构

#### Module_Board_Comm_t -板间通信设备结构体
```c
typedef struct
{
    uint8_t        initialized;//初始化标志位
    Can_Device    *candevice;  //板间通信CAN设备
    OfflineDevice *offline_dev;//板间通信掉线检测设备
#if defined(GIMBAL_BOARD)
    Chassis_Upload_Data_s chassis_upload_data;//定义为云台板，获取底盘板上传数据
#elif defined(CHASSIS_BOARD)
    Chassis_Ctrl_Cmd_s chassis_ctrl_cmd;//定义为底盘板，获取云台板遥控数据
#endif
} Module_Board_Comm_t;
```

#### Chassis_Ctrl_Cmd_s - 底盘控制指令（云台 -> 底盘）

| 字段 | 类型 | 说明 | 编码方式 |
| --- | --- | --- | --- |
| `vx` | float | 前向速度 | 缩放 10 倍后转为 `int8_t` |
| `vy` | float | 横向速度 | 缩放 10 倍后转为 `int8_t` |
| `wz` | float | 自旋角速度 | 缩放 10 倍后转为 `int8_t` |
| `offset_angle` | float | 底盘相对云台偏角 | 缩放 1000 倍后转为 `int16_t` |
| `chassis_mode` | enum | 底盘模式 | 原始字节映射 |

#### Chassis_Upload_Data_s - 底盘上报数据（底盘 -> 云台）
```c
typedef struct
{
    uint8_t reversed_1;
    uint8_t reversed_2;
    uint8_t reversed_3;
    uint8_t reversed_4;
    uint8_t reversed_5;
    uint8_t reversed_6;
    uint8_t reversed_7;
    uint8_t reversed_8;
} Chassis_Upload_Data_s;
```

---

## 3. 代码架构

```c
初始化层：
└─ Module_Board_comm_init()      // 根据宏定义自动配置接收/发送 ID

云台侧 API (GIMBAL_BOARD)：
├─ Module_Board_comm_send()      // 编码控制指令并发送至底盘
└─ Module_Board_comm_get_recv()  // 接收并解析底盘上报的状态

底盘侧 API (CHASSIS_BOARD)：
├─ Module_Board_comm_get_recv()  // 接收并解析来自云台的控制指令
└─ Module_Board_comm_send()      // 将底盘状态数据上报至云台

```

---

## 4. 协议转换细节（数据压缩）

为了在 CAN 的 8 字节负载中塞入尽可能多的浮点数信息，驱动采用了数据压缩：

1. 速度项：通过 `(float * 10.0f)` 映射，保留 1 位小数精度 。
2. 角度项：通过 `(float * 1000.0f)` 映射为 `int16_t`，跨两个字节存储，保留 3 位小数精度。

---

## 5. API 接口说明

```c
UINT Module_Board_comm_init(void);
```
```c
void Module_Board_comm_send(const T *data);
```
```c
UINT *Module_Board_comm_get_recv(uint32_t timeout);
```
---

## 6. 注意事项


- ID 匹配：初始化代码中 `CHASSIS_ID` 与 `GIMBAL_ID` 必须在不同板子上互为 Rx/Tx 关系。
- 模块依赖：本模块依赖BSP_CAN(通信方式)，Moudule_offline(掉线检测模块)
---

## 7.常见问题排查
1. 无法接收数据：
    - 排查方向
        1. 是否enable，使能board_comm
        2. 是否初始化board_comm
        3. board_comm是否掉线

## 8.使用示例
```c
//云台端
void App_robot_control_Init(void)
{
    status = Module_Board_comm_init();
    if (status != TX_SUCCESS)
    {
            log_e("board comm init failed!");
            return;
    }

}

void control_task(void)
{
    while(1) {
        // 周期性获取Remote数据并下发
        RemoteControlSet(&chassis_cmd,.....);
        Module_Board_comm_send(&chassis_cmd);
        // 接收chassis上传数据
        chassis_data = Module_Board_comm_get_recv(1000);

        tx_thread_sleep(2);
    }
}
```
```c
//底盘端
void App_robot_control_Init(void)
{
    status = Module_Board_comm_init();
    if (status != TX_SUCCESS)
    {
            log_e("board comm init failed!");
            return;
    }

}

void control_task(void)
{
    while(1) {
        // 上传chassis数据
        Module_Board_comm_send(&chassis_data);
        // 接收chassis命令
        chassis_cmd = Module_Board_comm_get_recv(1000);

        tx_thread_sleep(2);
    }
}

```
## 9. 版本历史

### v1.0 (2026-02-12)

- 初始版本。

---
