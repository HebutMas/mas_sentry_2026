/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-02-04 23:23:59
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-30 10:25:15
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\algorithm\lqr.c
 * @Description:
 */
#include "lqr.h"
#include "user_lib.h"

#ifndef abs
#define abs(x) ((x) > 0 ? (x) : -(x))
#endif

#include <arm_math.h>

/**
 * @brief 初始化LQR实例
 * @param lqr    LQR实例指针
 * @param config LQR初始化配置
 */
void LQRInit(LQRInstance *lqr, LQR_Init_Config_s *config)
{
    // 参数检查
    if (lqr == NULL || config == NULL)
    {
        return;
    }
    // 清零整个结构体
    memset(lqr, 0, sizeof(LQRInstance));
    // 初始化基本参数
    lqr->state_dim = config->state_dim;
    // 检查状态维度有效性
    if (lqr->state_dim < 1 || lqr->state_dim > 2)
    {
        return;
    }
    // 复制增益矩阵
    for (uint8_t i = 0; i < lqr->state_dim; i++)
    {
        lqr->K[i] = config->K[i];
    }
    // 初始化补偿参数
    lqr->comp_params        = config->comp_params;
    lqr->feedback_comp_rad  = config->feedback_comp_rad;
    // 初始化输出相关参数
    lqr->output_max = config->output_max;
    lqr->output_min = config->output_min;
}

static float calculateOutput(LQRInstance *lqr, float rad_degree, float rad_angular_velocity, float rad_ref)
{
    if (lqr == NULL)
    {
        return 0.0f;
    }

    float output;

    if (lqr->state_dim == 1)
    {
        // 单状态情况：角速度控制
        lqr->err = rad_angular_velocity - rad_ref;
        output   = -(lqr->K[0] * lqr->err);
    }
    else if (lqr->state_dim == 2)
    {
        // 双状态情况：角度和角速度控制
        lqr->err = rad_degree - rad_ref;
        output   = -(lqr->K[0] * lqr->err) - lqr->K[1] * rad_angular_velocity;
    }
    else
    {
        output = 0;
    }

    // 计算前馈补偿量 (重力补偿)，只有当参数有效时才计算
    float feedforward_output = 0.0f;
    if (lqr->comp_params != 0.0f)
    {
        feedforward_output = lqr->comp_params * arm_cos_f32(lqr->feedback_comp_rad != NULL ? *(lqr->feedback_comp_rad) : 0.0f);
    }

    lqr->output = output + feedforward_output;

    // 输出限幅
    VAL_LIMIT(lqr->output, lqr->output_min, lqr->output_max);

    return lqr->output;
}

float LQRCalculate(LQRInstance *lqr, float rad_degree, float rad_angular_velocity, float rad_ref)
{
    // 参数检查
    if (lqr == NULL || lqr->state_dim < 1 || lqr->state_dim > 2)
    {
        return 0.0f;
    }

    float output = calculateOutput(lqr, rad_degree, rad_angular_velocity, rad_ref);

    return output;
}