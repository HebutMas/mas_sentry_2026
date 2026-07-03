#include "module_w25q64.h"
#include "tx_api.h"
#include "tx_port.h"


#if defined(STM32H723xx)

#include "octospi.h"

#define LOG_TAG "module_w25q64"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

// clang-format off
#define W25Qxx_CMD_EnableReset    0x66 // 使能复位
#define W25Qxx_CMD_ResetDevice    0x99 // 复位器件
#define W25Qxx_CMD_JedecID        0x9F // JEDEC ID
#define W25Qxx_CMD_WriteEnable    0X06 // 写使能

#define W25Qxx_CMD_SectorErase    0x20 // 扇区擦除，4K字节， 参考擦除时间 45ms
#define W25Qxx_CMD_BlockErase_32K 0x52 // 块擦除，  32K字节，参考擦除时间 120ms
#define W25Qxx_CMD_BlockErase_64K 0xD8 // 块擦除，  64K字节，参考擦除时间 150ms
#define W25Qxx_CMD_ChipErase      0xC7 // 整片擦除，参考擦除时间 20S

#define W25Qxx_CMD_QuadInputPageProgram  0x32 // 1-1-4模式下(1线指令1线地址4线数据)，页编程指令，参考写入时间 0.4ms
#define W25Qxx_CMD_FastReadQuad_IO       0xEB // 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
#define W25Qxx_CMD_ReadStatus_REG1       0X05 // 读状态寄存器1
#define W25Qxx_Status_REG1_BUSY          0x01 // 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1
#define W25Qxx_Status_REG1_WEL           0x02 // 读状态寄存器1的第1位（只读），WEL写使能标志位，该标志位为1时，代表可以进行写操作

#define W25Qxx_PageSize              256        // 页大小，256字节
#define W25Qxx_FlashSize             0x800000   // W25Q64大小，8M字节
#define W25Qxx_FLASH_ID              0Xef4017   // W25Q64 JEDEC ID
#define W25Qxx_ChipErase_TIMEOUT_MAX 100000U    // 超时等待时间，W25Q64整片擦除所需最大时间是100S
#define W25Qxx_Mem_Addr              0x90000000 // 内存映射模式的地址
// clang-format on

/**
 * @description: 使用自动轮询标志查询，等待通信结束
 * @note 每一次通信都应该调用此函数，等待通信结束，避免错误的操作
 * @return {*}
 */
static UINT OSPI_W25Qxx_AutoPollingMemReady(void)
{
    OSPI_RegularCmdTypeDef  sCommand; // OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;  // 轮询比较相关配置参数

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;       // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;              // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;      // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;      // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE; // 禁止指令DTR模式
    sCommand.Address            = 0x0;                              // 地址0
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;            // 无地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;         // 地址长度24位
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;     // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;    //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;             // 1线数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;        // 禁止数据DTR模式
    sCommand.NbData             = 1;                                // 通信数据长度
    sCommand.DummyCycles        = 0;                                // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;             // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;     // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_ReadStatus_REG1; // 读状态信息寄存器

    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        log_e("Polling for a response that is not received");
        return TX_DELETED; // 轮询等待无响应
    }

    // 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_BUSY
    // 不停的与0作比较
    // 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1，空闲或通信结束为0
    // clang-format off
    sConfig.Match         = 0;                              //	匹配值
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;        //	与运算
    sConfig.Interval      = 0x10;                           //	轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE; // 自动停止模式
    sConfig.Mask          = W25Qxx_Status_REG1_BUSY; // 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位
    // clang-format on

    // 发送轮询等待命令
    if (HAL_OSPI_AutoPolling(&hospi2, &sConfig, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        log_e("Polling for a response that is not received");
        return TX_DELETED; // 轮询等待无响应
    }
    return TX_SUCCESS; // 通信正常结束
}

/**
 * @description: 初始化 OSPI 配置，读取器件ID
 * @return {uint32_t},读取到的器件ID，TX_DELETED - 通信、初始化错误
 */
static uint32_t OSPI_W25Qxx_ReadID(void)
{
    OSPI_RegularCmdTypeDef sCommand; // OSPI传输配置

    uint8_t  OSPI_ReceiveBuff[3]; // 存储OSPI读到的数据
    uint32_t W25Qxx_ID;           // 器件的ID

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;       // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;              // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;      // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;      // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE; // 禁止指令DTR模式
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;            // 无地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;         // 地址长度24位
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;    //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;             // 1线数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;        // 禁止数据DTR模式
    sCommand.NbData             = 3;                                // 传输数据的长度
    sCommand.DummyCycles        = 0;                                // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;             // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;     // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_JedecID; // 执行读器件ID命令

    HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE); // 发送指令

    HAL_OSPI_Receive(&hospi2, OSPI_ReceiveBuff, HAL_OSPI_TIMEOUT_DEFAULT_VALUE); // 接收数据

    W25Qxx_ID = (OSPI_ReceiveBuff[0] << 16) | (OSPI_ReceiveBuff[1] << 8) |
                OSPI_ReceiveBuff[2]; // 将得到的数据组合成ID

    return W25Qxx_ID; // 返回ID
}

/**
 * @description: 发送写使能命令
 * @return {UINT},TX_SUCEESS - 写使能成功，TX_DELETED - 写使能失败
 */
static UINT OSPI_W25Qxx_WriteEnable(void)
{
    OSPI_RegularCmdTypeDef  sCommand; // OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;  // 轮询比较相关配置参数

    sCommand.OperationType         = HAL_OSPI_OPTYPE_COMMON_CFG;           // 通用配置
    sCommand.FlashId               = HAL_OSPI_FLASH_ID_1;                  // flash ID
    sCommand.InstructionMode       = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize       = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode    = HAL_OSPI_INSTRUCTION_DTR_DISABLE;     // 禁止指令DTR模式
    sCommand.Address               = 0;                                    // 地址0
    sCommand.AddressMode           = HAL_OSPI_ADDRESS_NONE;                // 无地址模式
    sCommand.AddressSize           = HAL_OSPI_ADDRESS_24_BITS;             // 地址长度24位
    sCommand.AddressDtrMode        = HAL_OSPI_ADDRESS_DTR_DISABLE;         // 禁止地址DTR模式
    sCommand.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE; //	禁止替字节DTR模式
    sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;        //	无交替字节
    sCommand.DataMode              = HAL_OSPI_DATA_NONE;                   // 无数据模式
    sCommand.DataDtrMode           = HAL_OSPI_DATA_DTR_DISABLE;            // 禁止数据DTR模式
    sCommand.DummyCycles           = 0;                                    // 空周期个数
    sCommand.DQSMode               = HAL_OSPI_DQS_DISABLE;                 // 不使用DQS
    sCommand.SIOOMode              = HAL_OSPI_SIOO_INST_EVERY_CMD;         // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_WriteEnable; // 写使能命令

    // 发送写使能命令
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        log_e("Command transmission failed");
        return TX_DELETED;
    }
    // 发送查询状态寄存器命令
    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;       // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;              // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;      // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;      // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE; // 禁止指令DTR模式
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_NONE;            // 无地址模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;    //	无交替字节
    sCommand.DummyCycles        = 0;                                // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;             // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;     // 每次传输数据都发送指令
    sCommand.DataMode           = HAL_OSPI_DATA_1_LINE;             // 1线数据模式
    sCommand.NbData             = 1;                                // 通信数据长度

    sCommand.Instruction = W25Qxx_CMD_ReadStatus_REG1; // 查询状态寄存器命令

    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        log_e("Command transmission failed");
        return TX_DELETED;
    }

    // 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_WEL
    // 不停的与 0x02 作比较
    // 读状态寄存器1的第1位（只读），WEL写使能标志位，该标志位为1时，代表可以进行写操作
    // FANKE	7B0
    sConfig.Match         = 0x02;                           //	匹配值
    sConfig.MatchMode     = HAL_OSPI_MATCH_MODE_AND;        //	与运算
    sConfig.Interval      = 0x10;                           //	轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE; // 自动停止模式
    sConfig.Mask =
        W25Qxx_Status_REG1_WEL; // 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位

    if (HAL_OSPI_AutoPolling(&hospi2, &sConfig, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return TX_DELETED;
    }
    return TX_SUCCESS;
}
/**
 * @description: 进行块擦除操作，每次擦除64K字节
 * @param {uint32_t} SectorAddress,要擦除的地址
 * @return {*}
 */
static UINT OSPI_W25Qxx_BlockErase_64K(uint32_t SectorAddress)
{
    OSPI_RegularCmdTypeDef sCommand; // OSPI传输配置

    sCommand.OperationType      = HAL_OSPI_OPTYPE_COMMON_CFG;       // 通用配置
    sCommand.FlashId            = HAL_OSPI_FLASH_ID_1;              // flash ID
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;      // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;      // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE; // 禁止指令DTR模式
    sCommand.Address            = SectorAddress;                    // 地址
    sCommand.AddressMode        = HAL_OSPI_ADDRESS_1_LINE;          // 1线地址模式
    sCommand.AddressSize        = HAL_OSPI_ADDRESS_24_BITS;         // 地址长度24位
    sCommand.AddressDtrMode     = HAL_OSPI_ADDRESS_DTR_DISABLE;     // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;    //	无交替字节
    sCommand.DataMode           = HAL_OSPI_DATA_NONE;               // 无数据模式
    sCommand.DataDtrMode        = HAL_OSPI_DATA_DTR_DISABLE;        // 禁止数据DTR模式
    sCommand.DummyCycles        = 0;                                // 空周期个数
    sCommand.DQSMode            = HAL_OSPI_DQS_DISABLE;             // 不使用DQS
    sCommand.SIOOMode           = HAL_OSPI_SIOO_INST_EVERY_CMD;     // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_BlockErase_64K; // 扇区擦除指令，每次擦除64K字节

    // 发送写使能
    if (OSPI_W25Qxx_WriteEnable() != TX_SUCCESS)
    {
        return TX_DELETED; // 写使能失败
    }
    // 发送擦除指令
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return TX_DELETED; // 轮询等待无响应
    }
    // 使用自动轮询标志位，等待擦除的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != TX_SUCCESS)
    {
        return TX_DELETED; // 轮询等待无响应
    }
    return TX_SUCCESS; // 擦除成功
}

/**
 * @description: 按页写入，最大只能256字节
 * @param {uint8_t*} pBuffer，要写入的数据
 * @param {uint32_t} WriteAddr，要写入 W25Qxx 的地址
 * @param {uint16_t} NumByteToWrite，数据长度，最大只能256字节
 * @return {*}
 */
static UINT OSPI_W25Qxx_WritePage(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    OSPI_RegularCmdTypeDef sCommand; // OSPI传输配置

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG; // 通用配置
    sCommand.FlashId       = HAL_OSPI_FLASH_ID_1;        // flash ID

    sCommand.Instruction =
        W25Qxx_CMD_QuadInputPageProgram; // 1-1-4模式下(1线指令1线地址4线数据)，页编程指令
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;      // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;      // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE; // 禁止指令DTR模式

    sCommand.Address        = WriteAddr;                    // 地址
    sCommand.AddressMode    = HAL_OSPI_ADDRESS_1_LINE;      // 1线地址模式
    sCommand.AddressSize    = HAL_OSPI_ADDRESS_24_BITS;     // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE; // 禁止地址DTR模式

    sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;        // 无交替字节
    sCommand.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE; // 禁止替字节DTR模式

    sCommand.DataMode    = HAL_OSPI_DATA_4_LINES;     // 4线数据模式
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE; // 禁止数据DTR模式
    sCommand.NbData      = NumByteToWrite;            // 数据长度

    sCommand.DummyCycles = 0;                            // 空周期个数
    sCommand.DQSMode     = HAL_OSPI_DQS_DISABLE;         // 不使用DQS
    sCommand.SIOOMode    = HAL_OSPI_SIOO_INST_EVERY_CMD; // 每次传输数据都发送指令

    // 写使能
    if (OSPI_W25Qxx_WriteEnable() != TX_SUCCESS)
    {
        return TX_DELETED; // 写使能失败
    }
    // 写命令
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return TX_DELETED; // 传输数据错误
    }
    // 开始传输数据
    if (HAL_OSPI_Transmit(&hospi2, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return TX_DELETED; // 传输数据错误
    }
    // 使用自动轮询标志位，等待写入的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != TX_SUCCESS)
    {
        return TX_DELETED; // 轮询等待无响应
    }
    return TX_SUCCESS;
}

UINT Module_W25qxx_init(void)
{
    uint32_t Device_ID; // 器件ID

    Device_ID = OSPI_W25Qxx_ReadID(); // 读取器件ID

    if (Device_ID != W25Qxx_FLASH_ID) // 进行匹配
    {
        log_e("Module W25q64  init failed");
        return TX_DELETED;
    }

    log_i("Module W25q64 init success");

    return TX_SUCCESS;
}

UINT Module_W25Qxx_WriteBuffer(uint8_t *pBuffer, uint32_t Size)
{
    uint32_t end_addr, current_size, current_addr;
    uint8_t *write_data; // 要写入的数据

    // 写入之前，执行擦除
    if (OSPI_W25Qxx_BlockErase_64K(0) != TX_SUCCESS)
    {
        return TX_DELETED;
    }

    current_size = W25Qxx_PageSize - (0 % W25Qxx_PageSize); // 计算当前页还剩余的空间

    if (current_size > Size) // 判断当前页剩余的空间是否足够写入所有数据
    {
        current_size = Size; // 如果足够，则直接获取当前长度
    }

    current_addr = 0;        // 获取要写入的地址
    end_addr     = 0 + Size; // 计算结束地址
    write_data   = pBuffer;  // 获取要写入的数据

    do
    {
        // 按页写入数据
        if (OSPI_W25Qxx_WritePage(write_data, current_addr, current_size) != TX_SUCCESS)
        {
            log_e("Page writing failed");
            return TX_DELETED;
        }

        else // 按页写入数据成功，进行下一次写数据的准备工作
        {
            current_addr += current_size; // 计算下一次要写入的地址
            write_data += current_size;   // 获取下一次要写入的数据存储区地址
            // 计算下一次写数据的长度
            current_size = ((current_addr + W25Qxx_PageSize) > end_addr) ? (end_addr - current_addr)
                                                                         : W25Qxx_PageSize;
        }
    } while (current_addr < end_addr); // 判断数据是否全部写入完毕

    return TX_SUCCESS;
}

UINT Module_W25Qxx_ReadBuffer(uint8_t *pBuffer, uint32_t NumByteToRead)
{
    OSPI_RegularCmdTypeDef sCommand; // OSPI传输配置

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG; // 通用配置
    sCommand.FlashId       = HAL_OSPI_FLASH_ID_1;        // flash ID

    sCommand.Instruction =
        W25Qxx_CMD_FastReadQuad_IO; // 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
    sCommand.InstructionMode    = HAL_OSPI_INSTRUCTION_1_LINE;      // 1线指令模式
    sCommand.InstructionSize    = HAL_OSPI_INSTRUCTION_8_BITS;      // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE; // 禁止指令DTR模式

    sCommand.Address        = 0;                            // 地址
    sCommand.AddressMode    = HAL_OSPI_ADDRESS_4_LINES;     // 4线地址模式
    sCommand.AddressSize    = HAL_OSPI_ADDRESS_24_BITS;     // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE; // 禁止地址DTR模式

    sCommand.AlternateBytesMode    = HAL_OSPI_ALTERNATE_BYTES_NONE;        // 无交替字节
    sCommand.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE; // 禁止替字节DTR模式

    sCommand.DataMode    = HAL_OSPI_DATA_4_LINES;     // 4线数据模式
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE; // 禁止数据DTR模式
    sCommand.NbData      = NumByteToRead;             // 数据长度

    sCommand.DummyCycles = 6;                            // 空周期个数
    sCommand.DQSMode     = HAL_OSPI_DQS_DISABLE;         // 不使用DQS
    sCommand.SIOOMode    = HAL_OSPI_SIOO_INST_EVERY_CMD; // 每次传输数据都发送指令

    // 写命令
    if (HAL_OSPI_Command(&hospi2, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return TX_DELETED; // 传输数据错误
    }
    //	接收数据
    if (HAL_OSPI_Receive(&hospi2, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return TX_DELETED; // 传输数据错误
    }
    // 使用自动轮询标志位，等待接收的结束
    if (OSPI_W25Qxx_AutoPollingMemReady() != TX_SUCCESS)
    {
        return TX_DELETED; // 轮询等待无响应
    }
    return TX_SUCCESS;
}

#elif defined(STM32F407xx)

UINT Module_W25qxx_init(void){
    return TX_DELETED;
}
UINT Module_W25Qxx_WriteBuffer(uint8_t *pBuffer, uint32_t Size){
    return TX_DELETED;
}     
UINT Module_W25Qxx_ReadBuffer(uint8_t *pBuffer, uint32_t NumByteToRead){
    return TX_DELETED;
}
#endif
