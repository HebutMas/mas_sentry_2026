/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-25 21:02:31
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-25 21:41:58
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/MOTOR/motor_register.c
 * @Description:
 */
#include "motor_register.h"

// 最大支持32个电机（对应32位event flag）
// 指针数组：存储对应 eventflag 位的电机地址
void *g_motor_ptr_map[32] = {NULL};
// 类型数组：存储该位置是 DM 还是 DJI，还是其他电机，以便解析时强转指针
Motor_Type_e g_motor_type_map[32] = {MOTOR_TYPE_NONE};



UINT MotorRegistryRegister(void *motor_ptr, uint32_t eventflag, Motor_Type_e type)
{
    if (eventflag == 0 || motor_ptr == NULL) return TX_DELETED;

    // 计算 flag 的索引位 (例如 flag=0x04 -> index=2)
    // 使用 GCC 内置函数计算末尾0的个数，效率极高
    uint8_t index = (uint8_t)__builtin_ctz(eventflag);

    if (index < 32)
    {
        g_motor_ptr_map[index] = motor_ptr;
        g_motor_type_map[index] = type;
        return TX_SUCCESS;
    }
    return TX_DELETED;
}

// 查找指针
void *MotorRegistryLookup(uint32_t eventflag)
{
    if (eventflag == 0) return NULL;

    uint8_t index = (uint8_t)__builtin_ctz(eventflag);

    if (index < 32)
    {
        return g_motor_ptr_map[index];
    }
    return NULL;
}

// 查找类型
Motor_Type_e MotorRegistryGetType(uint32_t eventflag)
{
    if (eventflag == 0) return MOTOR_TYPE_NONE;

    uint8_t index = (uint8_t)__builtin_ctz(eventflag);

    if (index < 32)
    {
        return g_motor_type_map[index];
    }
    return MOTOR_TYPE_NONE;
}