/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-19 18:45:37
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-19 19:30:08
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/W25Q64/module_w25q64.h
 * @Description:
 */
#ifndef _MODULE_W25Q64_H_
#define _MODULE_W25Q64_H_

#include "tx_port.h"
#include <stdint.h>


/**
 * @description: w25qxx初始化
 * @return {*}
 */
UINT Module_W25qxx_init(void);        
/**
 * @description: 写入数据
 * @note 最大不能超过flash芯片的大小或者64k，默认从地址0开始写
 * @param {uint8_t} *pBuffer，要写入的数据
 * @param {uint32_t} Size，数据长度
 * @return {*}
 */ 
UINT Module_W25Qxx_WriteBuffer(uint8_t *pBuffer, uint32_t Size);         
/**
 * @description: 读取数据
 * @param {uint8_t} *pBuffer,要读取的数据
 * @param {uint32_t} NumByteToRead,数据长度，最大不能超过flash芯片的大小
 * @return {*}
 */
UINT Module_W25Qxx_ReadBuffer(uint8_t *pBuffer, uint32_t NumByteToRead);     

#endif // _MODULE_W25Q64_H_