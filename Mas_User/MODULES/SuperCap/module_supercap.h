#ifndef _MODULE_SUPERCAP_H_
#define _MODULE_SUPERCAP_H_

#include "bsp_can.h"
#include "module_offline.h"
#include <stdint.h>

#if defined(STM32H723xx)
#define SUPERCAP_CAN &hfdcan2
#elif defined(STM32F407xx)
#define SUPERCAP_CAN &hcan1
#endif

#pragma pack(1)

typedef struct
{
    uint8_t  enableDCDC    : 1;        // 是否启用DCDC
    uint8_t  systemRestart : 1;        // 是否重启系统
    uint8_t  reserved      : 6;        // 保留位，必须为0
    uint16_t feedbackJudgePowerLimit;  // 反馈功率限制
    uint16_t feedbackJudgePowerBuffer; // 反馈功率缓冲
    uint8_t  reserved2[3];             // 保留字节，必须为0
} SuperCap_Send_t;

typedef struct
{
    uint8_t  errorCode;
    float    chassispower;
    uint16_t chassispower_limit;
    uint8_t  capEnergy;
} SuperCap_Receive_t;

#pragma pack()

typedef struct
{
    uint8_t            initialized;  // 是否初始化完成
    SuperCap_Receive_t receive_data; // 接收的数据
    Can_Device        *device;       // CAN设备
    OfflineDevice     *offline_dev;  // 离线设备
} Module_SuperCap_t;

UINT Module_SuperCap_Init(void);

void Module_SuperCap_func();

void Module_SuperCap_Send(const SuperCap_Send_t *data);

Module_SuperCap_t *Module_SuperCap_Get(void);

#endif // _MODULE_SUPERCAP_H_