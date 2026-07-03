/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-17 17:02:53
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-04-01 13:05:03
 * @FilePath: \mas_rm_djic\Mas_User\BSP\USB\bsp_cdc_acm.h
 * @Description:
 */
#ifndef _BSP_CDC_ACM_H_
#define _BSP_CDC_ACM_H_

#include "tx_port.h"
#include <stdint.h>

#pragma pack(1)
/* 虚拟串口传输数据定义 */

// 发送结构体
typedef struct
{
    uint8_t  header;
    uint8_t  mode;
    float    q[4];                            // wxyz顺序
    uint16_t projectile_allowance_17mm;       // 剩余发弹量
    uint8_t  power_management_shooter_output; // 功率管理 shooter 输出
    uint16_t current_hp;                      // 机器人当前血量
    uint16_t outpost_HP;                      // 前哨站血量
    uint16_t base_HP;                         // 基地血量
    uint8_t  game_progess;
    uint8_t  tail;
} SendPacket;

// 接收结构体
typedef struct
{
    uint8_t header;
    uint8_t found;
    uint8_t fire_advice;
    float   target_yaw;
    float   target_pitch;
    float   vx;
    float   vy;
    uint8_t nav_state;
    uint8_t tail;
} ReceivePacket;

#pragma pack()

/**
 * @brief 初始化CDC ACM USB设备
 * @param busid USB总线ID
 * @param reg_base USB外设寄存器基地址
 * @return 无返回值
 */
void cdc_acm_init(uint8_t busid, uintptr_t reg_base);
/**
 * @brief 注册USB离线设备
 * @return 无返回值
 */
void cdc_acm_offline_register(void);
/**
 * @brief 发送数据
 * @param sendpacket 指向要发送数据的指针
 * @param timeout 超时时间
 * @return 无返回值
 */
void cdc_acm_data_send(SendPacket *sendpacket, uint32_t timeout);
/**
 * @brief 读取数据
 * @return ReceivePacket* 指向最新接收数据的指针，NULL表示无数据
 */
ReceivePacket *cdc_acm_read_data(void);
/**
 * @brief 获取USB是否离线状态
 * @return uint8_t USB是否离线状态
 */
uint8_t cdc_acm_usb_offline_state(void);

#endif // _BSP_CDC_ACM_H_s