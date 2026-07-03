#ifndef _BSP_FLASH_H_
#define _BSP_FLASH_H_

#include "tx_port.h"
#include <stdint.h>


/**
 * @description: 在指定地址写入数据块
 * @param {uint32_t} address - 写入地址
 * @param {uint8_t*} data - 要写入的数据指针
 * @param {uint32_t} size - 数据大小(字节)
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_FLASH_Write_Buffer(uint8_t *buffer, uint32_t length);
/**
 * @description: 从指定地址读取数据块
 * @param {uint32_t} address - 读取地址
 * @param {uint8_t*} data - 读取到的数据指针
 * @param {uint32_t} size - 数据大小(字节)
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_FLASH_Read_Buffer(uint8_t *buffer, uint32_t length);

#endif // _BSP_FLASH_H_