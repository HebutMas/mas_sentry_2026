/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-13 12:42:44
 * @LastEditors: laladuduqq 17503181697@163.com
 * @LastEditTime: 2026-03-26 22:07:15
 * @FilePath: \mas_rm_djic\Mas_User\BSP\bsp_init.c
 * @Description:
 */
#include "bsp_init.h"
#include "bsp_can.h"
#include "bsp_dwt.h"
#include "gpio.h"
#include "bsp_cdc_acm.h"

#define LOG_TAG "bsp_init"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

void BSP_Init(void)
{
#if defined(STM32H723xx)
    // POWER_24V 和 POWER_5V 电源使能
    HAL_GPIO_WritePin(POWER_24V_1_GPIO_Port, POWER_24V_1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(POWER_24V_2_GPIO_Port, POWER_24V_2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(POWER_5V_GPIO_Port, POWER_5V_Pin, GPIO_PIN_SET);

    DWT_Init(480);
    //reset cdc_acm
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    DWT_Delay(0.1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    DWT_Delay(0.1);
    cdc_acm_init(0, USB_OTG_HS_PERIPH_BASE);
    Can_Event_init();
#elif defined(STM32F407xx)
    DWT_Init(168);
    // reset cdc_acm
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    DWT_Delay(0.1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
    DWT_Delay(0.1);
    cdc_acm_init(0, USB_OTG_FS_PERIPH_BASE);
    Can_Event_init();
#endif
    log_i("robot init success");
}
