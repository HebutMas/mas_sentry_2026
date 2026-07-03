#ifndef _MODULE_INS_H_
#define _MODULE_INS_H_

#include <stdint.h>

// INS结构体
typedef struct
{
    // 四元数估计值
    float q[4];
    // 机体坐标加速度
    float MotionAccel_b[3];
    // 绝对系加速度
    float MotionAccel_n[3];
    // 加速度低通滤波系数
    float AccelLPF;
    // bodyframe在绝对系的向量表示
    float xn[3];
    float yn[3];
    float zn[3];
    // 加速度在机体系和XY两轴的夹角
    // float atanxz;
    // float atanyz;
    // 位姿
    float   euler_rad[3];  // roll pitch yaw
    float   YawTotalAngle_rad;
    int32_t YawRoundCount;
    // 采样时间间隔
    float    dt;
    uint32_t dwt_cnt;
    // 初始化标志
    uint8_t initialized;
} Ins_t;

/* 用于修正安装误差的参数 */
typedef struct
{
    uint8_t flag;
    float   scale[3];
    float   Yaw;
    float   Pitch;
    float   Roll;
} Ins_param_t;

/**
 * @description: 初始化INS模块
 * @return {*}
 */
void Module_Ins_init();
/**
 * @description: 获取Ins_t指针
 * @return {Ins_t *}，返回Ins_t指针
 */
Ins_t *Module_Ins_get();
/**
 * @description: ins主任务函数
 * @return {*}
 */
void Module_Ins_task_func();

#endif // _MODULE_INS_H_