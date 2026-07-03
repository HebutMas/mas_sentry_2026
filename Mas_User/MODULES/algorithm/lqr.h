#ifndef __LQR_H
#define __LQR_H
#include "stdint.h"

// 前馈补偿函数指针类型
typedef float (*FeedforwardFunc)(float ref, float degree, float angular_velocity);

// LQR错误类型枚举
typedef enum
{
    LQR_ERROR_NONE = 0,
    LQR_MOTOR_BLOCKED_ERROR
} LQRErrorType;

typedef struct
{
    float   K[2];               // LQR增益矩阵 K
    uint8_t state_dim;          // 状态维度
    float   output_max;         // 输出最大值
    float   output_min;         // 输出最小值
    float   comp_params;        // 补偿参数配置
    float  *feedback_comp_rad;  // 反馈补偿角度指针
} LQR_Init_Config_s;

typedef struct
{
    float   K[2];      // LQR增益矩阵 K
    uint8_t state_dim; // 状态维度
    float   err;
    float   output;
    float   state[2];
    float   output_max; // 输出最大值
    float   output_min; // 输出最小值

    float  comp_params;        // 补偿参数配置
    float *feedback_comp_rad;  // 反馈补偿角度指针
} LQRInstance;

/**
 * @brief 初始化LQR实例
 * @param lqr    LQR实例指针
 * @param config LQR初始化配置
 */
void LQRInit(LQRInstance *lqr, LQR_Init_Config_s *config);

/**
 * @brief 计算LQR输出
 * @param lqr             LQR实例指针
 * @param rad_degree      角度（弧度）
 * @param rad_angular_velocity  角速度（弧度）
 * @param rad_ref         参考值（弧度）
 * @return float          LQR计算输出
 */
float LQRCalculate(LQRInstance *lqr, float rad_degree, float rad_angular_velocity, float rad_ref);

#endif