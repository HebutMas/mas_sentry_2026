/*
 * @Author: laladuduqq 17503181697@163.com
 * @Date: 2026-03-28 22:51:02
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-04-02 19:22:23
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\SuperCap\module_supercap.c
 * @Description:
 */

#include "module_supercap.h"

#define LOG_TAG "module_supercap"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static Module_SuperCap_t module_supercap;

UINT Module_SuperCap_Init(void)
{
    Can_Device_Init_Config_s board_com_init = {
        .hcan  = SUPERCAP_CAN,
        .rx_id = 0X051,
        .tx_id = 0X061,
    };
    module_supercap.device = BSP_CAN_Device_Init(&board_com_init);
    if (module_supercap.device == NULL)
    {
        log_e("can device init failed");
        return TX_DELETED;
    }
    Offline_Init_config_t offline_manage_init = {.name = "supercap", .timeout_ms = 500, .beep_times = 9, .enable = OFFLINE_DISABLE};
    module_supercap.offline_dev               = Module_Offline_device_register(&offline_manage_init);
    if (module_supercap.offline_dev == NULL)
    {
        log_e("offline device init failed");
        return TX_DELETED;
    }

    module_supercap.initialized = 1;

    return TX_SUCCESS;
}

void Module_SuperCap_Send(const SuperCap_Send_t *data)
{
    if (module_supercap.initialized && data != NULL)
    {
        uint8_t buf[8];
        memcpy(buf, data, sizeof(SuperCap_Send_t));
        BSP_CAN_SendDevice(module_supercap.device, buf, sizeof(SuperCap_Send_t));
    }
}

void Module_SuperCap_func()
{
    if (!module_supercap.initialized)
    {
        tx_thread_sleep(100);
    }

    if (BSP_CAN_ReadSingleDevice(module_supercap.device, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        memcpy(&module_supercap.receive_data, module_supercap.device->rx_buf, sizeof(SuperCap_Receive_t));
        Module_Offline_device_update(module_supercap.offline_dev);
    }
}

Module_SuperCap_t *Module_SuperCap_Get(void)
{
    if (module_supercap.initialized)
    {
        return &module_supercap;
    }
    return NULL;
}