#include "referee_protocol.h"
#include "bsp_def.h"
#include "module_offline.h"
#include "crc_rm.h"
#include <string.h>

#define LOG_TAG "module_referee"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static Module_Referee_s                                    module_referee;
BUFFER_SECTION __attribute__((aligned(32))) static uint8_t rx_buffer[2][512];
BUFFER_SECTION __attribute__((aligned(32))) static uint8_t tx_buffer[256];

void Module_Referee_Init()
{
    UART_Device_init_config config = {.huart           = &REFEREE_UART,
                                      .expected_rx_len = 0,
                                      .rx_buf          = (uint8_t (*)[2])rx_buffer,
                                      .rx_buf_size     = 512,
                                      .tx_mode         = UART_MODE_DMA,
                                      .rx_mode         = UART_MODE_DMA};
    module_referee.uart_dev        = BSP_UART_Device_Init(&config);
    if (module_referee.uart_dev == NULL)
    {
        log_e("uart device init error");
        return;
    }
    Offline_Init_config_t offlineconfig = {.name = "referee", .beep_times = REFEREE_OFFLINE_BEEP_TIMES, .enable = OFFLINE_ENABLE, .timeout_ms = 100};
    module_referee.offline_dev          = Module_Offline_device_register(&offlineconfig);
    if (module_referee.offline_dev == NULL)
    {
        log_e("offline device register error");
        return;
    }

    module_referee.initialized = 1;

    log_i("referee module initialized");
}

void Module_Referee_Send_Interaction(uint16_t sub_cmd_id, uint16_t sender_id, uint16_t recv_id, const void *payload, uint16_t payload_len,
                                     uint32_t timeout)
{
    if (module_referee.initialized == 0 || payload == NULL || payload_len == 0 || payload_len > 112u) return;

    // 交互数据段长度 = 6字节头（sub_cmd_id+sender+recv）+ payload
    const uint16_t data_len = 6u + payload_len;
    uint16_t       offset   = 0;

    // 帧头
    tx_buffer[offset++] = REFEREE_SOF;
    tx_buffer[offset++] = (uint8_t)(data_len & 0xFF);
    tx_buffer[offset++] = (uint8_t)(data_len >> 8);
    tx_buffer[offset++] = module_referee.tx_seq++;
    tx_buffer[offset++] = Get_CRC8_Check_Sum(tx_buffer, 4, 0xFF); // CRC8

    // 命令码 0x0301
    tx_buffer[offset++] = (uint8_t)(CMD_ID_ROBOT_INTERACTION & 0xFF);
    tx_buffer[offset++] = (uint8_t)(CMD_ID_ROBOT_INTERACTION >> 8);

    // 交互帧头：sub_cmd_id + sender_id + recv_id
    tx_buffer[offset++] = (uint8_t)(sub_cmd_id & 0xFF);
    tx_buffer[offset++] = (uint8_t)(sub_cmd_id >> 8);
    tx_buffer[offset++] = (uint8_t)(sender_id & 0xFF);
    tx_buffer[offset++] = (uint8_t)(sender_id >> 8);
    tx_buffer[offset++] = (uint8_t)(recv_id & 0xFF);
    tx_buffer[offset++] = (uint8_t)(recv_id >> 8);

    // 用户数据
    memcpy(&tx_buffer[offset], payload, payload_len);
    offset += payload_len;

    // CRC16
    uint16_t crc16      = Get_CRC16_Check_Sum(tx_buffer, offset, 0xFFFF);
    tx_buffer[offset++] = (uint8_t)(crc16 & 0xFF);
    tx_buffer[offset++] = (uint8_t)(crc16 >> 8);

    BSP_UART_Send(module_referee.uart_dev, tx_buffer, offset, timeout);
}

void Module_Referee_Decode_func()
{
    if (!module_referee.initialized)
    {
        tx_thread_sleep(100);
        return;
    }

    uint8_t *buf = BSP_UART_Read(module_referee.uart_dev, TX_WAIT_FOREVER);
    if (buf == NULL || module_referee.uart_dev->real_rx_len == 0) return;

    uint16_t       idx     = 0;
    const uint16_t rx_len  = module_referee.uart_dev->real_rx_len;
    const uint16_t MIN_LEN = FRAME_HEADER_LEN + CMD_ID_LEN + FRAME_TAIL_LEN;

    while (idx < rx_len)
    {
        const uint8_t *p   = buf + idx;
        uint16_t       rem = rx_len - idx;

        // 最短帧长检查，不足则退出
        if (rem < MIN_LEN) break;

        // SOF 对齐：非帧头则跳过该字节
        if (p[0] != REFEREE_SOF)
        {
            idx++;
            continue;
        }

        // 取 data_length，并做合法性检查
        uint16_t data_length = (uint16_t)(p[1] | (p[2] << 8));
        if (data_length > 118u)
        {
            idx++;
            continue;
        }

        // 帧完整性检查：数据不足则等待下次
        uint16_t frame_len = FRAME_HEADER_LEN + CMD_ID_LEN + data_length + FRAME_TAIL_LEN;
        if (rem < frame_len) break;

        // CRC8 校验帧头（前4字节）
        if (Get_CRC8_Check_Sum((uint8_t *)p, 4, 0xFF) != p[4])
        {
            idx++;
            continue;
        }

        // CRC16 校验整包
        uint16_t crc16_recv = (uint16_t)(p[frame_len - 2] | (p[frame_len - 1] << 8));
        if (Get_CRC16_Check_Sum((uint8_t *)p, frame_len - 2u, 0xFFFF) != crc16_recv)
        {
            idx++;
            continue;
        }

        // 解析命令码与数据
        uint16_t       cmd_id  = (uint16_t)(p[5] | (p[6] << 8));
        const uint8_t *payload = p + FRAME_HEADER_LEN + CMD_ID_LEN;

        switch (cmd_id)
        {
        case CMD_ID_GAME_STATUS:
            memcpy(&module_referee.game_status, payload, sizeof(game_status_t));
            break;
        case CMD_ID_ROBOT_HP:
            memcpy(&module_referee.robot_hp, payload, sizeof(robot_hp_data_t));
            break;
        case CMD_ID_ROBOT_STATUS:
            memcpy(&module_referee.robot_status, payload, sizeof(robot_status_t));
            break;
        case CMD_ID_POWER_HEAT_DATA:
            memcpy(&module_referee.power_heat, payload, sizeof(power_heat_data_t));
            break;
        case CMD_ID_BUFF_DATA:
            memcpy(&module_referee.buff, payload, sizeof(buff_t));
            break;
        case CMD_ID_SHOOT_DATA:
            memcpy(&module_referee.shoot_data, payload, sizeof(shoot_data_t));
            break;
        case CMD_ID_ALLOWED_BULLET:
            memcpy(&module_referee.allowed_bullet, payload, sizeof(allowed_bullet_t));
            break;
        case CMD_ID_SENTRY_AUTO_SYNC:
            memcpy(&module_referee.sentry_sync, payload, sizeof(sentry_auto_sync_t));
            break;
        default:
            break;
        }

        Module_Offline_device_update(module_referee.offline_dev);

        idx += frame_len;
    }
}

uint8_t *Module_Referee_Get_cmd_data(uint16_t cmd_id)
{
    if (module_referee.initialized == 0)
    {
        return NULL;
    }
    switch (cmd_id)
    {
    case CMD_ID_GAME_STATUS:
        return (uint8_t *)&module_referee.game_status;
    case CMD_ID_ROBOT_HP:
        return (uint8_t *)&module_referee.robot_hp;
    case CMD_ID_ROBOT_STATUS:
        return (uint8_t *)&module_referee.robot_status;
    case CMD_ID_POWER_HEAT_DATA:
        return (uint8_t *)&module_referee.power_heat;
    case CMD_ID_BUFF_DATA:
        return (uint8_t *)&module_referee.buff;
    case CMD_ID_SHOOT_DATA:
        return (uint8_t *)&module_referee.shoot_data;
    case CMD_ID_ALLOWED_BULLET:
        return (uint8_t *)&module_referee.allowed_bullet;
    case CMD_ID_SENTRY_AUTO_SYNC:
        return (uint8_t *)&module_referee.sentry_sync;
    default:
        return NULL;
    }
}