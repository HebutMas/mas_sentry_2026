/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-03-12 19:53:17
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-29 01:14:19
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\REFEREE\referee_user_ui.c
 * @Description:
 */
#include "referee_user_ui.h"
#include "referee_protocol_ui.h"
#include <string.h>
#include <math.h>

// 剩余子弹状态
static uint16_t s_remain_bullet_val = 0;

// g0：低频/静态图元
static ui_figure_7_t s_frame_g0;
#define s_aim_line      (&s_frame_g0.figure[0]) // AIM 准线（静态，init后不变）
#define s_chassis_round (&s_frame_g0.figure[1]) // 底盘模式圆
#define s_frict_round   (&s_frame_g0.figure[2]) // 摩擦轮圆
#define s_remain_bullet (&s_frame_g0.figure[3]) // 剩余子弹整型
#define s_cap_arc       (&s_frame_g0.figure[4]) // 电容功率弧
#define s_cap_round     (&s_frame_g0.figure[5]) // 电容状态圆
#define s_cap_int       (&s_frame_g0.figure[6]) // 电容剩余容量整型

// g1：高频动态图元
static ui_figure_5_t s_frame_g1;
#define s_autoround   (&s_frame_g1.figure[0]) // 中心大圆（auto_aim联动）
#define s_pitch_float (&s_frame_g1.figure[1]) // Pitch 浮点
#define s_align_line  (&s_frame_g1.figure[2]) // 对准圆弧
#define s_yaw_float   (&s_frame_g1.figure[3]) // Yaw 浮点

static ui_string_t s_str_autoaim;   // auto_aim 状态字符串（动态更改）
static ui_string_t s_str_supercap;  // "SUPERCAP"  layer0 粉色 @ (28,733)   font=20
static ui_string_t s_str_chassis;   // "CHASSIS"   layer0 粉色 @ (46,876)   font=20
static ui_string_t s_str_firct;     // "FIRCT"     layer0 粉色 @ (88,807)   font=20
static ui_string_t s_str_yaw;       // "YAW"       layer0 橙色 @ (927,189)  font=20
static ui_string_t s_str_pitch;     // "PITCH"     layer0 橙色 @ (1240,553) font=20
static ui_string_t s_str_no_bullet; // "NO BULLET" 无子弹时覆盖显示，layer1 粉色 @ (892,860) font=24

static uint8_t         s_shooter_pwr_online = 1; // 默认认为已供电，防止启动时错误覆盖
static chassis_mode_e  s_last_chassis       = (chassis_mode_e)0xFF;
static friction_mode_e s_last_frict         = (friction_mode_e)0xFF;
static uint8_t         s_last_no_bullet     = 0xFF; // 0=有子弹 1=无子弹
static uint16_t        s_last_bullet        = 0xFFFF;
static int32_t         s_last_pitch_val     = 0x7FFFFFFF;
static int32_t         s_last_yaw_val       = 0x7FFFFFFF;
static uint16_t        s_last_cap_pct       = 0xFFFF;
static uint8_t         s_last_auto_aim      = 0xFF; // auto_aim_online
static float           s_last_align_angle   = 1e9f; // alginangle
static uint8_t         s_last_shooter_pwr   = 0xFF; // shooter 24V 供电状态

void referee_ui_init(void)
{
    // g0：低频/静态图元

    // AIM：准线竖线，绿色 width=20 layer0
    ui_draw_line(s_aim_line, "g00", UI_OP_ADD, 0, REFEREE_UI_COLOR_GREEN, 20, 950, 490, 950, 514);

    // CHASSISRound：底盘模式圆，黄色 r=21 layer1
    ui_draw_circle(s_chassis_round, "g01", UI_OP_ADD, 1, REFEREE_UI_COLOR_YELLOW, 10, 239, 864, 21);

    // FIRCTRound：摩擦轮圆，黄色 r=21 layer1
    ui_draw_circle(s_frict_round, "g02", UI_OP_ADD, 1, REFEREE_UI_COLOR_YELLOW, 10, 235, 795, 21);

    // REMAINGBULLET：剩余子弹整型，粉色 font=24 layer1
    ui_draw_int(s_remain_bullet, "g03", UI_OP_ADD, 1, REFEREE_UI_COLOR_PINK, 2, 24, 892, 860, 0);

    // CAPPOWER：电容功率弧，绿色 layer2 ang=200~340 rx=161 ry=342
    ui_draw_arc(s_cap_arc, "g04", UI_OP_ADD, 2, REFEREE_UI_COLOR_GREEN, 28, 611, 534, 200, 340, 161, 342);

    // SUPERCAPRound：电容状态圆，黄色 r=22 layer1
    ui_draw_circle(s_cap_round, "g05", UI_OP_ADD, 1, REFEREE_UI_COLOR_YELLOW, 10, 236, 722, 22);

    // CAP_INT：电容剩余容量整型，圆弧上方，黄色 font=24 layer2
    ui_draw_int(s_cap_int, "g06", UI_OP_ADD, 2, REFEREE_UI_COLOR_YELLOW, 2, 24, 565, 175, 0);

    ui_send_figure_7(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_frame_g0);

    // g1：高频动态图元

    // AUTOROUND：中心大圆，粉色 r=391 layer3
    ui_draw_circle(s_autoround, "g07", UI_OP_ADD, 3, REFEREE_UI_COLOR_PINK, 5, 961, 538, 391);

    // PITCHFloat：pitch 角浮点，黄色 font=30 layer2
    ui_draw_float(s_pitch_float, "g08", UI_OP_ADD, 2, REFEREE_UI_COLOR_YELLOW, 3, 30, 2, 1469, 560, 0);

    // ALIGINGIMBAL：对准圆弧，0度在正上方，弧宽30度，橙色 width=30 layer1
    ui_draw_arc(s_align_line, "g09", UI_OP_ADD, 1, REFEREE_UI_COLOR_ORANGE, 30, 961, 538, 345, 15, 391, 391);

    // YAWFloat：yaw 角浮点，黄色 font=30 layer2
    ui_draw_float(s_yaw_float, "g0a", UI_OP_ADD, 2, REFEREE_UI_COLOR_YELLOW, 3, 30, 2, 873, 130, 0);

    ui_send_figure_5(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_frame_g1);

    // 字符串标签
    // YAW label
    ui_draw_string(&s_str_yaw, "g1a", UI_OP_ADD, 0, REFEREE_UI_COLOR_ORANGE, 2, 20, 927, 189, "YAW");
    ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_yaw);

    // PITCH label
    ui_draw_string(&s_str_pitch, "g1b", UI_OP_ADD, 0, REFEREE_UI_COLOR_ORANGE, 2, 20, 1240, 553, "PITCH");
    ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_pitch);

    // AUTO_AIM label（初始化为 OFFLINE）
    ui_draw_string(&s_str_autoaim, "g0c", UI_OP_ADD, 0, REFEREE_UI_COLOR_PINK, 2, 20, 874, 905, "OFFLINE");
    ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_autoaim);

    // SUPERCAP label
    ui_draw_string(&s_str_supercap, "g0d", UI_OP_ADD, 0, REFEREE_UI_COLOR_PINK, 2, 20, 28, 733, "SUPERCAP");
    ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_supercap);

    // CHASSIS label
    ui_draw_string(&s_str_chassis, "g0e", UI_OP_ADD, 0, REFEREE_UI_COLOR_PINK, 2, 20, 46, 876, "CHASSIS");
    ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_chassis);

    // FIRCT label
    ui_draw_string(&s_str_firct, "g0f", UI_OP_ADD, 0, REFEREE_UI_COLOR_PINK, 2, 20, 88, 807, "FIRCT");
    ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_firct);
}

void referee_ui_update_by_user(chassis_mode_e chassis, friction_mode_e frict, int32_t pitch_x1000, int32_t yaw_x1000, float alginangle, uint16_t pct,
                               uint8_t auto_aim_online)
{
    // 底盘模式圆颜色
    if (chassis != s_last_chassis)
    {
        s_last_chassis = chassis;
        uint8_t color;
        switch (chassis)
        {
        case CHASSIS_FOLLOW_GIMBAL_YAW:
            color = REFEREE_UI_COLOR_GREEN;
            break;
        case CHASSIS_ROTATE:
            color = REFEREE_UI_COLOR_YELLOW;
            break;
        case CHASSIS_ROTATE_REVERSE:
            color = REFEREE_UI_COLOR_ORANGE;
            break;
        case CHASSIS_ZERO_FORCE:
        default:
            color = REFEREE_UI_COLOR_PINK;
            break;
        }
        ui_draw_circle(s_chassis_round, "g01", UI_OP_MODIFY, 1, color, 10, 239, 864, 21);
        ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_chassis_round);
    }

    // 摩擦轮圆颜色（仅在 shooter 已供电时才刷新）
    if (frict != s_last_frict && s_shooter_pwr_online)
    {
        s_last_frict   = frict;
        uint8_t fcolor = (frict == FRICTION_ON) ? REFEREE_UI_COLOR_GREEN : REFEREE_UI_COLOR_PINK;
        ui_draw_circle(s_frict_round, "g02", UI_OP_MODIFY, 1, fcolor, 10, 235, 795, 21);
        ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_frict_round);
    }

    // AUTOROUND 和 AUTO_AIM 字符串：auto_aim_online状态
    // 0=离线(默认黑圈) 1=AUTO_AIM(粉色) 2=SMALL_BUFF(黄色) 3=BIG_BUFF(橙色)
    if (auto_aim_online != s_last_auto_aim)
    {
        s_last_auto_aim = auto_aim_online;
        uint8_t     circle_color;
        uint8_t     str_color;
        const char *aim_str;
        switch (auto_aim_online)
        {
        case 1:
            circle_color = REFEREE_UI_COLOR_PINK;
            str_color    = REFEREE_UI_COLOR_PINK;
            aim_str      = "AUTO_AIM";
            break;
        case 2:
            circle_color = REFEREE_UI_COLOR_YELLOW;
            str_color    = REFEREE_UI_COLOR_YELLOW;
            aim_str      = "SMALL_BUFF";
            break;
        case 3:
            circle_color = REFEREE_UI_COLOR_ORANGE;
            str_color    = REFEREE_UI_COLOR_ORANGE;
            aim_str      = "BIG_BUFF";
            break;
        case 4:
            circle_color = REFEREE_UI_COLOR_GREEN;
            str_color    = REFEREE_UI_COLOR_GREEN;
            aim_str      = "FOUND_TARGET";
            break;
        case 0:
            circle_color = REFEREE_UI_COLOR_BLACK;
            str_color    = REFEREE_UI_COLOR_PINK;
            aim_str      = "OFFLINE";
            break;
        }
        ui_draw_circle(s_autoround, "g07", UI_OP_MODIFY, 3, circle_color, 5, 961, 538, 391);
        ui_draw_string(&s_str_autoaim, "g0c", UI_OP_MODIFY, 0, str_color, 2, 20, 874, 905, aim_str);
        ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_autoround);
        ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_autoaim);
    }

    // ALIGINGIMBAL：圆弧随 alginangle 旋转，0度=12点钟，顺时针，弧宽30度
    if (fabsf(alginangle - s_last_align_angle) > 5.0f)
    {
        s_last_align_angle = alginangle;
        int32_t center     = (int32_t)alginangle % 360;
        if (center < 0) center += 360;
        int32_t sa = center - 15;
        int32_t ea = center + 15;
        if (sa < 0) sa += 360;
        if (ea >= 360) ea -= 360;
        ui_draw_arc(s_align_line, "g09", UI_OP_MODIFY, 1, REFEREE_UI_COLOR_ORANGE, 30, 961, 538, (uint16_t)sa, (uint16_t)ea, 391, 391);
        ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_align_line);
    }

    // Pitch + Yaw 浮点
    uint8_t pitch_chg = (pitch_x1000 != s_last_pitch_val);
    uint8_t yaw_chg   = (yaw_x1000 != s_last_yaw_val);

    if (pitch_chg || yaw_chg)
    {
        s_last_pitch_val = pitch_x1000;
        s_last_yaw_val   = yaw_x1000;
        ui_figure_set_int_value(s_pitch_float, pitch_x1000);
        ui_figure_set_op(s_pitch_float, UI_OP_MODIFY);
        ui_figure_set_int_value(s_yaw_float, yaw_x1000);
        ui_figure_set_op(s_yaw_float, UI_OP_MODIFY);
        ui_figure_2_t batch_py;
        batch_py.figure[0] = *s_pitch_float;
        batch_py.figure[1] = *s_yaw_float;
        ui_send_figure_2(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &batch_py);
    }

    // 电容功率弧 + SUPERCAPRound圆 + 电容容量整型
    // SUPERCAPRound：pct==0 → 黑色，pct>0 → 绿色
    if (pct != s_last_cap_pct)
    {
        s_last_cap_pct     = pct;
        // 将0-255范围的pct值映射到0-100的显示范围
        uint16_t safe_pct  = pct > 255u ? 100u : (uint16_t)((uint32_t)pct * 100u / 255u);
        uint16_t end_ang   = (uint16_t)(200u + (uint32_t)safe_pct * 140u / 100u);
        uint8_t  arc_color = (safe_pct < 60u) ? REFEREE_UI_COLOR_YELLOW : REFEREE_UI_COLOR_GREEN;
        uint8_t  rnd_color = (safe_pct == 0u) ? REFEREE_UI_COLOR_BLACK : REFEREE_UI_COLOR_GREEN;
        ui_draw_arc(s_cap_arc, "g04", UI_OP_MODIFY, 2, arc_color, 28, 611, 534, 200, end_ang, 161, 342);
        ui_draw_circle(s_cap_round, "g05", UI_OP_MODIFY, 1, rnd_color, 10, 236, 722, 22);
        ui_figure_set_int_value(s_cap_int, (int32_t)pct);
        ui_figure_set_op(s_cap_int, UI_OP_MODIFY);
        // 圆弧 + 圆 + 整型共3图，用 5图帧打包
        ui_figure_5_t batch_cap;
        batch_cap.figure[0] = *s_cap_arc;
        batch_cap.figure[1] = *s_cap_round;
        batch_cap.figure[2] = *s_cap_int;
        memset(&batch_cap.figure[3], 0, sizeof(ui_figure_t));
        memset(&batch_cap.figure[4], 0, sizeof(ui_figure_t));
        ui_send_figure_5(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &batch_cap);
    }
}

void referee_ui_update_by_referee(void)
{
    // 从裁判系统拉取允许发弹量
    allowed_bullet_t *ab = (allowed_bullet_t *)Module_Referee_Get_cmd_data(CMD_ID_ALLOWED_BULLET);
    if (ab != NULL) s_remain_bullet_val = ab->bullet_17mm_allowed;
    if (s_remain_bullet_val < 0)
    {
        s_remain_bullet_val = 0;
    }
    // shooter 24V 供电状态→摩擦轮圆附加黑色覆盖
    robot_status_t *rs = (robot_status_t *)Module_Referee_Get_cmd_data(CMD_ID_ROBOT_STATUS);
    if (rs != NULL)
    {
        uint8_t shooter_pwr = rs->power_output.shooter_output;
        if (shooter_pwr != s_last_shooter_pwr)
        {
            s_last_shooter_pwr = shooter_pwr;
            if (shooter_pwr == 0u)
            {
                // shooter 断电
                s_shooter_pwr_online = 0u;
                ui_draw_circle(s_frict_round, "g02", UI_OP_MODIFY, 1, REFEREE_UI_COLOR_BLACK, 10, 235, 795, 21);
                ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_frict_round);
                // 不重置s_last_frict，避免update_by_user覆盖
            }
            else
            {
                // shooter 供电恢复
                s_shooter_pwr_online = 1u;
                s_last_frict         = (friction_mode_e)0xFF;
            }
        }
    }

    // 剩余子弹：0 时显示 "NO BULLET" 字符串，否则显示整型数字
    if (s_remain_bullet_val != s_last_bullet)
    {
        s_last_bullet        = s_remain_bullet_val;
        uint8_t is_no_bullet = (s_remain_bullet_val == 0u) ? 1u : 0u;

        if (is_no_bullet != s_last_no_bullet)
        {
            s_last_no_bullet = is_no_bullet;
            if (is_no_bullet)
            {
                ui_draw_string(&s_str_no_bullet, "g0n", UI_OP_ADD, 1, REFEREE_UI_COLOR_PINK, 2, 24, 892, 860, "NO BULLET");
                ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_no_bullet);
                ui_figure_set_op(s_remain_bullet, UI_OP_DELETE);
                ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_remain_bullet);
            }
            else
            {
                ui_figure_set_op(&s_str_no_bullet.figure_config, UI_OP_DELETE);
                ui_send_string(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, &s_str_no_bullet);
                ui_draw_int(s_remain_bullet, "g03", UI_OP_ADD, 1, REFEREE_UI_COLOR_PINK, 2, 24, 892, 860, (int32_t)s_remain_bullet_val);
                ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_remain_bullet);
            }
        }
        else if (!is_no_bullet)
        {
            ui_figure_set_int_value(s_remain_bullet, (int32_t)s_remain_bullet_val);
            ui_figure_set_op(s_remain_bullet, UI_OP_MODIFY);
            ui_send_figure_1(UI_SELF_ROBOT_ID, UI_SELF_CLIENT_ID, s_remain_bullet);
        }
    }
}