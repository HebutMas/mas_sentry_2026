#include "power_control.h"
#include "motor_dji.h"
#include "user_lib.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define PC_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define PC_ABS(x)           ((x) >= 0.0f ? (x) : -(x))

static PIDInstance bufferpid;

// 电机节点
typedef struct
{
    DJI_Motor_t         *motor_ptr; /* 电机指针*/
    Motor_Type_e         motor_type;
    PowerCtrl_Role_e     role;  /* 驱动轮 or 舵向轮 */
    PowerControl_Param_t param; /* 损耗模型参数及中间结果 */
} PC_Motor_Node_t;

static PC_Motor_Node_t s_motors[POWER_CTRL_MAX_MOTOR_NUM];
static uint8_t         s_motor_cnt = 0;

static float   s_power_limit       = 45.0f; /* 裁判系统功率上限 (W)  */
static float   s_buffer_energy     = 60.0f; /* 当前缓冲能量 (J)      */
static uint8_t s_buffer_energy_use = 1;     /* 是否允许使用缓冲能量  */

// 辅助函数

static float DJI_GetCurrentA(const DJI_Motor_t *m)
{
    switch (m->motor_info.motor_type)
    {
    case M3508:
        return IntegerToCurrent(-20.0f, 20.0f, -16384, 16384, (int16_t)m->motor_controller.output);
    case M2006:
        return IntegerToCurrent(-10.0f, 10.0f, -10000, 10000, (int16_t)m->motor_controller.output);
    case GM6020_CURRENT:
        return IntegerToCurrent(-3.0f, 3.0f, -16384, 16384, (int16_t)m->motor_controller.output);
    default:
        return 0.0f;
    }
}

static float DJI_CurrentA_To_Output(float current_A, Motor_Type_e type)
{
    switch (type)
    {
    case M3508:
        return (float)currentToInteger(-20.0f, 20.0f, -16384, 16384, current_A);
    case M2006:
        return (float)currentToInteger(-10.0f, 10.0f, -10000, 10000, current_A);
    case GM6020_CURRENT:
        return (float)currentToInteger(-3.0f, 3.0f, -16384, 16384, current_A);
    default:
        return 0.0f;
    }
}

static void SetMotorTorque(PC_Motor_Node_t *node, float torque_Nm)
{
    DJI_Motor_t *m       = node->motor_ptr;
    float        Kt_gear = m->motor_info.torque_constant * m->motor_info.gear_ratio;
    if (Kt_gear < 1e-6f) return;
    float current_A            = torque_Nm / Kt_gear;
    m->motor_controller.output = (float)DJI_CurrentA_To_Output(current_A, node->motor_ptr->motor_info.motor_type);
    node->param.output         = m->motor_controller.output;
}

// 对外接口

void PowerControl_Set(float power_limit, float buffer_energy, uint8_t buffer_energy_use)
{
    
    s_power_limit       = power_limit;
    s_buffer_energy     = buffer_energy;
    s_buffer_energy_use = buffer_energy_use;
}

void PowerControl_Motor_Init(DJI_Motor_t *motor, PowerCtrl_Role_e role, PowerControl_Param_t param)
{
    if (s_motor_cnt >= POWER_CTRL_MAX_MOTOR_NUM || motor == NULL) return;

    PC_Motor_Node_t *node = &s_motors[s_motor_cnt++];
    node->motor_ptr       = motor;
    node->motor_type      = motor->motor_info.motor_type;
    node->role            = role;
    node->param           = param;

    PID_Init_Config_s pid_config = {.MaxOut = 50, .IntegralLimit = 0.0, .DeadBand = 5, .Kp = 5, .Ki = 0, .Kd = 0, .Improve = 0x01}; // enable integratiaon limit
    PIDInit(&bufferpid, &pid_config);
}

/**
 * @brief 对指定角色的电机组执行混合权重功率分配
 * @param role_filter  只处理该角色的电机 (PC_ROLE_DRIVE 或 PC_ROLE_STEER)
 * @param max_power    本组可用功率上限 (W)
 * @param cmd_sum_out  [out] 该组电机命令功率之和 (W)，可为 NULL
 */
static void PowerGroup_Limit(PowerCtrl_Role_e role_filter, float max_power, float *cmd_sum_out)
{
    // 检查本组是否有电机
    uint8_t N = 0;
    for (uint8_t i = 0; i < s_motor_cnt; i++)
        if (s_motors[i].role == role_filter) N++;
    if (N == 0)
    {
        if (cmd_sum_out) *cmd_sum_out = 0.0f;
        return;
    }

    // 收集本组电机数据
    float omega[POWER_CTRL_MAX_MOTOR_NUM];  /* 输出轴角速度 (rad/s)       */
    float tau[POWER_CTRL_MAX_MOTOR_NUM];    /* 输出轴扭矩   (N·m)         */
    float cmdPow[POWER_CTRL_MAX_MOTOR_NUM]; /* 估算功率     (W)           */
    float errRad[POWER_CTRL_MAX_MOTOR_NUM]; /* |目标-实测| 角速度 (rad/s) */

    float sum_cmd_power = 0.0f;
    float allocatable   = 0.0f;
    float sum_pos_power = 0.0f;
    float sum_error     = 0.0f;

    for (uint8_t i = 0; i < s_motor_cnt; i++)
    {
        PC_Motor_Node_t *node = &s_motors[i];

        if (node->role != role_filter)
        {
            omega[i] = tau[i] = cmdPow[i] = errRad[i] = 0.0f;
            continue;
        }

        float k1 = node->param.k1;
        float k2 = node->param.k2;
        float k3 = node->param.k3;

        // 输出轴角速度 = 转子角速度 / 减速比
        omega[i] = node->motor_ptr->measure.speed_rad / node->motor_ptr->motor_info.gear_ratio;
        // 输出轴扭矩 = I × Kt × gear_ratio
        tau[i] = DJI_GetCurrentA(node->motor_ptr) * node->motor_ptr->motor_info.torque_constant * node->motor_ptr->motor_info.gear_ratio;

        /* P_i = τ·Ω + k1·|Ω| + k2·τ² + k3 */
        cmdPow[i] = tau[i] * omega[i] + k1 * PC_ABS(omega[i]) + k2 * (tau[i] * tau[i]) + k3;

        node->param.raw_power = cmdPow[i];
        sum_cmd_power += cmdPow[i];

        // 速度误差: 转子目标角速度与实测角速度之差 (rad/s)
        errRad[i] = PC_ABS(node->motor_ptr->motor_controller.ref - node->motor_ptr->measure.speed_rad);

        if (cmdPow[i] <= 0.0f)
            allocatable += (-cmdPow[i]);
        else
        {
            sum_pos_power += cmdPow[i];
            sum_error += errRad[i];
        }
    }

    if (cmd_sum_out) *cmd_sum_out = sum_cmd_power;

    // 不超限
    if (sum_cmd_power <= max_power)
    {
        for (uint8_t i = 0; i < s_motor_cnt; i++)
        {
            if (s_motors[i].role != role_filter) continue;
            s_motors[i].param.assigned_power = cmdPow[i];
            DJI_Motor_t *dm                  = s_motors[i].motor_ptr;
            s_motors[i].param.output         = DJI_GetCurrentA(dm) * dm->motor_info.torque_constant * dm->motor_info.gear_ratio;
        }
        return;
    }

    // 超限
    allocatable += max_power;

    // 误差置信度: 误差越大越倾向"按误差分配"
    float error_confidence;
    if (sum_error >= POWER_CTRL_ERROR_THRESHOLD)
        error_confidence = 1.0f;
    else if (sum_error > POWER_CTRL_PROP_THRESHOLD)
        error_confidence = (sum_error - POWER_CTRL_PROP_THRESHOLD) / (POWER_CTRL_ERROR_THRESHOLD - POWER_CTRL_PROP_THRESHOLD);
    else
        error_confidence = 0.0f;

    for (uint8_t i = 0; i < s_motor_cnt; i++)
    {
        PC_Motor_Node_t *node = &s_motors[i];
        if (node->role != role_filter) continue;

        float k1 = node->param.k1;
        float k2 = node->param.k2;
        float k3 = node->param.k3;

        if (cmdPow[i] <= 0.0f)
        {
            // 保持原始输出
            node->param.assigned_power = cmdPow[i];
            continue;
        }

        // 混合权重: 误差置信度插值
        float w_error = (sum_error > 1e-5f) ? (errRad[i] / sum_error) : (1.0f / (float)N);
        float w_prop  = (sum_pos_power > 1e-5f) ? (cmdPow[i] / sum_pos_power) : (1.0f / (float)N);
        float w       = error_confidence * w_error + (1.0f - error_confidence) * w_prop;

        float p_alloc = w * allocatable;

        /* 悬空轮保护：对速度误差极小的电机（已接近目标，无需额外发力），
         * 限制其分配功率不超过其自身当前估算消耗 cmdPow[i]。
         * 这样悬空轮（tau≈0，errRad≈0）不会抢占接地轮的功率预算。 */
        /* 100 RPM 转换为转子侧 rad/s: 100 * 2π / 60 ≈ 10.47 rad/s */
        if (errRad[i] < 10.47f && cmdPow[i] < p_alloc) p_alloc = cmdPow[i];

        node->param.assigned_power = p_alloc;

        /* 反解扭矩 τ:
         *   k2·τ² + Ω·τ + (k1|Ω| + k3 - p_alloc) = 0
         */
        float Omega  = omega[i];
        float a_coef = k2;
        float b_coef = Omega;
        float c_coef = k1 * PC_ABS(Omega) + k3 - p_alloc;

        float tau_new;
        if (a_coef < 1e-8f)
        {
            tau_new = (PC_ABS(Omega) > 1e-5f) ? (p_alloc - k1 * PC_ABS(Omega) - k3) / Omega : 0.0f;
        }
        else
        {
            float delta = b_coef * b_coef - 4.0f * a_coef * c_coef;
            if (delta < 0.0f)
            {
                tau_new = -b_coef / (2.0f * a_coef);
            }
            else
            {
                float sqrt_delta = sqrtf(delta);
                float root1      = (-b_coef + sqrt_delta) / (2.0f * a_coef);
                float root2      = (-b_coef - sqrt_delta) / (2.0f * a_coef);
                /* 选根：按目标速度方向选取符号一致的根 */
                tau_new = (node->motor_ptr->motor_controller.output >= 0.0f) ? root1 : root2;
            }
        }

        float max_tau = node->motor_ptr->motor_info.max_torque;
        tau_new       = PC_CLAMP(tau_new, -max_tau, max_tau);
        SetMotorTorque(node, tau_new);
    }
}

void PowerControl_Update(void)
{
    if (s_motor_cnt == 0) return;

    // 可用功率上限设置
    float effective_limit = s_power_limit;
    if (s_buffer_energy_use)
    {
        // 缓冲 > 50 J 时允许超额
        PIDCalculate(&bufferpid, s_buffer_energy, 30.0f);
        effective_limit -= bufferpid.Output;
    }

    // 检测是否有舵向轮注册
    uint8_t has_steer = 0;
    for (uint8_t i = 0; i < s_motor_cnt; i++)
        if (s_motors[i].role == PC_ROLE_STEER)
        {
            has_steer = 1;
            break;
        }

    if (!has_steer)
    {
        // 麦轮 / 全向轮: 全部驱动轮纳入单组功率分配
        PowerGroup_Limit(PC_ROLE_DRIVE, effective_limit, NULL);
    }
    else
    {
        // 舵轮 / 半舵: 舵向轮先分配，剩余功率给驱动轮
        float steer_limit   = effective_limit * POWER_CTRL_STEER_RATIO;
        float steer_cmd_sum = 0.0f;

        PowerGroup_Limit(PC_ROLE_STEER, steer_limit, &steer_cmd_sum);

        /* 舵向轮实际消耗 = min(命令功率总和, 舵向上限) */
        float steer_actual = (steer_cmd_sum < steer_limit) ? steer_cmd_sum : steer_limit;
        float drive_limit  = effective_limit - steer_actual;
        if (drive_limit < 0.0f) drive_limit = 0.0f;

        PowerGroup_Limit(PC_ROLE_DRIVE, drive_limit, NULL);
    }
}

float PowerControl_Get(DJI_Motor_t *motor, Motor_Type_e motor_type)
{
    for (uint8_t i = 0; i < s_motor_cnt; i++)
    {
        if (s_motors[i].motor_ptr == motor && s_motors[i].motor_type == motor_type)
        {
            return s_motors[i].param.output;
        }
    }
    return 0.0f;
}
