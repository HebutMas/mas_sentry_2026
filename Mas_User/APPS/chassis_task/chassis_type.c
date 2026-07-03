#include "chassis_type.h"
#include "motor_dji.h"
#include "robot_config.h"

#define TWO_PI (2.0f * PI)
#define HALF_PI (PI / 2.0f)

/**
 * @brief 角度回环
 * @note 将任意角度限制在 +/- max/2 范围内，防止溢出
 */
static void AngleLoop_f(float *angle, float max)
{
    while ((*angle < -(max / 2)) || (*angle > (max / 2)))
    {
        if (*angle < -(max / 2))
            *angle += max;
        else if (*angle > (max / 2))
            *angle -= max;
    }
}


void Chassis_Calculate_Target(DJI_Motor_t *motors[8], float vx, float vy, float vw)
{
    // 对准值数组
    const float align_rad[4] = {
        CHASSIS_STEER_ALIGN_RAD_LF, 
        CHASSIS_STEER_ALIGN_RAD_LB,
        CHASSIS_STEER_ALIGN_RAD_RB, 
        CHASSIS_STEER_ALIGN_RAD_RF
    };

    // 上一次的目标角度，用于零速保持
    static float last_target_angle_rad[4] = {0};

    // 零速标志
    uint8_t is_zero_speed = (fabsf(vx) < 0.01f && fabsf(vy) < 0.01f && fabsf(vw) < 0.01f);

    for (int i = 0; i < 4; i++)
    {
        float local_vx = 0, local_vy = 0;

        // 运动学分解：将底盘速度分解到各个轮子的局部坐标系
        switch (i)
        {
        case 0: // 左前
            local_vx = vx + vw * (WHEEL_R * 0.70710678f);
            local_vy = vy + vw * (WHEEL_R * 0.70710678f);
            break;
        case 1: // 左后
            local_vx = vx + vw * (WHEEL_R * 0.70710678f);
            local_vy = vy - vw * (WHEEL_R * 0.70710678f);
            break;
        case 2: // 右后
            local_vx = vx - vw * (WHEEL_R * 0.70710678f);
            local_vy = vy - vw * (WHEEL_R * 0.70710678f);
            break;
        case 3: // 右前
            local_vx = vx - vw * (WHEEL_R * 0.70710678f);
            local_vy = vy + vw * (WHEEL_R * 0.70710678f);
            break;
        }

        // 计算驱动电机 (3508) 目标速度
        float velocity_vector = sqrtf(local_vx * local_vx + local_vy * local_vy);
        // 线速度 -> 轮子角速度 -> 电机轴角速度
        float target_speed_rad = (velocity_vector / RADIUS_WHEEL_M) * CHASSIS_DECELE_RATIO;

        // 计算转向电机 (6020) 目标角度
        float  target_steer_rad;
        int8_t drct_factor = 1; // 方向因子，1为正转，-1为翻转(对应180度转向)

        if (is_zero_speed)
        {
            // 如果底盘静止，保持上一次的角度
            target_steer_rad = last_target_angle_rad[i];
        }
        else
        {
            // 计算目标矢量角度 (atan2 返回值范围是 -PI 到 PI)
            float vector_rad = atan2f(local_vy, local_vx);

            // 目标物理角度 = 轮子机械零点 + 目标矢量角度
            float target_abs_angle = align_rad[i] + vector_rad;

            // 规范化到 [-PI, PI] 范围
            AngleLoop_f(&target_abs_angle, TWO_PI);

            // 获取当前转向电机的单圈电机角度
            float current_angle = motors[i + 4]->measure.angle_single_round;

            // 计算目标角度与当前角度的差值
            float diff = target_abs_angle - current_angle;
            AngleLoop_f(&diff, TWO_PI); // 差值也规范化到 [-PI, PI]

            target_steer_rad = target_abs_angle;

            // 路径规划：如果旋转角度超过 90度 (HALF_PI)，则选择反向旋转 180度
            if (diff > HALF_PI)
            {
                target_steer_rad -= PI; // 目标减去 180度
                drct_factor = -1;       // 轮子反转
            }
            else if (diff < -HALF_PI)
            {
                target_steer_rad += PI; // 目标加上 180度
                drct_factor = -1;       // 轮子反转
            }

            // 最终规范化目标角度
            AngleLoop_f(&target_steer_rad, TWO_PI);

            // 记录本次目标，供零速时使用
            last_target_angle_rad[i] = target_steer_rad;
        }

        // 发送控制指令

        // 驱动电机：速度 * 方向因子
        Motor_DJI_SetRef(motors[i], target_speed_rad * (float)drct_factor);

        // 转向电机：发送目标弧度
        float delta = target_steer_rad - motors[i+4]->measure.total_angle;
        AngleLoop_f(&delta, TWO_PI); 
        Motor_DJI_SetRef(motors[i+4], motors[i+4]->measure.total_angle + delta);
    }
}