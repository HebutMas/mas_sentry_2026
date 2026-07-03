#ifndef _BSP_SPI_H_
#define _BSP_SPI_H_

#include "bsp_def.h"
#include "spi.h"
#include "tx_api.h"

#define SPI_BUS_NUM                2 // 总线数量

/* SPI事件定义 */
#define SPI_EVENT_TX_DONE_EVENT    (0x01 << 0)
#define SPI_EVENT_RX_DONE_EVENT    (0x01 << 1)
#define SPI_EVENT_TX_RX_DONE_EVENT (0x01 << 2)
#define SPI_EVENT_ERROR_EVENT      (0x01 << 3)

/* 传输模式枚举 */
typedef enum
{
    SPI_MODE_BLOCKING,
    SPI_MODE_IT,
    SPI_MODE_DMA
} SPI_Mode;

/* SPI设备实例结构体 */
typedef struct SPI_Device
{
    uint32_t           valid;   // 设备指针有效性检测标志
    SPI_HandleTypeDef *hspi;    // SPI句柄
    GPIO_TypeDef      *cs_port; // 片选端口（NULL表示无CS引脚控制）
    uint16_t           cs_pin;  // 片选引脚（0表示无CS引脚控制）
    SPI_Mode           tx_mode; // 发送模式
    SPI_Mode           rx_mode; // 接收模式
    struct SPI_Device *next;    // 链表下一个节点指针
} SPI_Device;

/* 初始化配置结构体 */
typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port; // 片选端口（NULL表示无CS引脚控制）
    uint16_t           cs_pin;  // 片选引脚（0表示无CS引脚控制）
    SPI_Mode           tx_mode;
    SPI_Mode           rx_mode;
} SPI_Device_Init_Config;

/* SPI总线管理结构 */
typedef struct SPI_Bus_Manager
{
    SPI_HandleTypeDef   *hspi;        // SPI句柄
    SPI_Device          *device_list; // 设备链表头指针
    TX_EVENT_FLAGS_GROUP bus_event;   // 总线事件
    TX_MUTEX             bus_mutex;   // 总线互斥量
} SPI_Bus_Manager;

/**
 * @description:  初始化SPI设备
 * @param {SPI_Device_Init_Config*} config，初始化配置，见SPI_Device_Init_Config结构体
 * @return {SPI_Device*} 成功返回设备实例指针，失败返回NULL
 */
SPI_Device *BSP_SPI_Device_Init(SPI_Device_Init_Config *config);
/**
 * @description:  销毁SPI设备实例
 * @param {SPI_Device*} dev，要销毁的设备实例
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_SPI_Device_DeInit(SPI_Device *dev);
/**
 * @description: SPI发送和接受数据
 * @details      发送数据并接收响应，适用于全双工通信
 * @details      如果只需要发送或接收，请使用BSP_SPI_Transmit或BSP_SPI_Receive
 * @param {SPI_Device*} dev，要操作的SPI设备实例
 * @param {uint8_t*} tx_data，发送数据指针
 * @param {uint8_t*} rx_data，接收数据指针
 * @param {uint16_t} size，数据大小，注意这里是发送和接收同时进行，所以发送和接收的数据量是相等的
 * @param {uint32_t} timeout，超时时间
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_SPI_TransReceive(SPI_Device *dev, const uint8_t *tx_data, uint8_t *rx_data, uint16_t size,
                          uint32_t timeout);
/**
 * @description: SPI发送数据，适用于单向发送数据
 * @param {SPI_Device*} dev，要操作的SPI设备实例
 * @param {uint8_t*} tx_data，发送数据指针
 * @param {uint16_t} size，数据大小
 * @param {uint32_t} timeout，超时时间
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_SPI_Transmit(SPI_Device *dev, const uint8_t *tx_data, uint16_t size, uint32_t timeout);
/**
 * @description: SPI接收数据，适用于单向接收数据
 * @param {SPI_Device*} dev，要操作的SPI设备实例
 * @param {uint8_t*} rx_data，接收数据指针
 * @param {uint16_t} size，数据大小
 * @param {uint32_t} timeout，超时时间
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_SPI_Receive(SPI_Device *dev, uint8_t *rx_data, uint16_t size, uint32_t timeout);
/**
 * @description: SPI连续发送两次数据，适用于需要连续发送两次数据的场景
 * @param {SPI_Device*} dev，要操作的SPI设备实例
 * @param {const uint8_t*} tx_data1，第一次发送的数据指针
 * @param {uint16_t} size1，第一次发送的数据大小
 * @param {const uint8_t*} tx_data2，第二次发送的数据指针
 * @param {uint16_t} size2，第二次发送的数据大小
 * @param {uint32_t} timeout，超时时间
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_SPI_TransAndTrans(SPI_Device *dev, const uint8_t *tx_data1, uint16_t size1,
                           const uint8_t *tx_data2, uint16_t size2, uint32_t timeout);
/**
 * @description: SPI连续接收两次数据，适用于需要连续接收两次数据的场景
 * @param {SPI_Device*} dev，要操作的SPI设备实例
 * @param {uint8_t*} rx_data1，第一次接收的数据指针
 * @param {uint16_t} size1，第一次接收的数据大小
 * @param {uint8_t*} rx_data2，第二次接收的数据指针
 * @param {uint16_t} size2，第二次接收的数据大小
 * @param {uint32_t} timeout，超时时间
 * @return {UINT} 返回操作状态,TX_SUCCESS表示成功，其他值表示失败
 */
UINT BSP_SPI_ReceAndRece(SPI_Device *dev, uint8_t *rx_data1, uint16_t size1, uint8_t *rx_data2,
                         uint16_t size2, uint32_t timeout);

#endif // _BSP_SPI_H_
