#include "bsp_flash.h"
#include "tx_api.h"
#include <string.h>
#include "stm32f4xx_hal.h"

#define User_Flash_Sector      0x080E0000U
#define User_Flash_Sector_Size (128U * 1024U)

#define BSP_FLASH_BASE_ADDR 0x08000000U

/* 内部函数：根据地址自动擦除对应的扇区 */
static UINT BSP_FLASH_Erase_UserSector(void)
{
    UINT                   ret       = TX_SUCCESS;
    uint32_t               PageError = 0;
    FLASH_EraseInitTypeDef EraseInitStruct;

    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V to 3.6V
    EraseInitStruct.NbSectors    = 1;                     // 每次只擦除 1 个扇区
    EraseInitStruct.Sector = FLASH_SECTOR_11;

    /* 调用 HAL 擦除函数 */
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        ret = TX_DELETED;
    }

    return ret;
}

UINT BSP_FLASH_Read_Buffer(uint8_t *buffer, uint32_t length)
{
    // 防止读取越界
    if (buffer == NULL || length > User_Flash_Sector_Size)
    {
        return TX_DELETED;
    }

    memcpy(buffer, (uint8_t *)User_Flash_Sector, length);

    return TX_SUCCESS;
}

UINT BSP_FLASH_Write_Buffer(uint8_t *buffer, uint32_t length)
{
    HAL_StatusTypeDef status = HAL_OK;
    UINT              ret    = TX_SUCCESS;

    if (buffer == NULL || length == 0 || length > User_Flash_Sector_Size)
        return TX_DELETED;

    // 解锁
    HAL_FLASH_Unlock();

    // 擦除对应扇区
    if (BSP_FLASH_Erase_UserSector() != TX_SUCCESS)
    {
        HAL_FLASH_Lock();
        return TX_DELETED;
    }
    uint32_t current_addr = User_Flash_Sector;
    uint8_t *data_ptr     = buffer;

    for (uint32_t i = 0; i < length; i++)
    {
        if (*(__IO uint8_t *)current_addr != 0xFF)
        {
            ret = TX_DELETED;
            break;
        }
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, current_addr, *data_ptr);
        if (status != HAL_OK)
        {
            ret = TX_DELETED;
            break;
        }
        current_addr++;
        data_ptr++;
    }

    HAL_FLASH_Lock();
    return ret;
}