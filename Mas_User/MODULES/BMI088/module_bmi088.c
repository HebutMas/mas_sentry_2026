/*
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2025-09-11 13:43:09
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-23 15:18:17
 * @FilePath: /MAS_RM_DAMIAOH7/Mas_User/MODULES/BMI088/module_bmi088.c
 * @Description:
 */
#include "module_bmi088.h"
#include "BMI088_reg.h"
#include "bsp_dwt.h"
#include "bsp_pwm.h"
#include "pid.h"
#include <stdint.h>
#include <string.h>

#if defined(STM32H723xx)
#include "module_w25q64.h"
#elif defined(STM32F407xx)
#include "bsp_flash.h"
#endif

#define LOG_TAG "module_bmi088"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

static Bmi088_device_t bmi088_device;

// 常量定义
#define BMI088REG   0
#define BMI088DATA  1
#define BMI088ERROR 2
// BMI088初始化配置数组for accel,第一列为reg地址,第二列为写入的配置值,第三列为错误码(如果出错)
static uint8_t BMI088_Accel_Init_Table[BMI088_WRITE_ACCEL_REG_NUM][3] = {
    {BMI088_ACC_PWR_CTRL, BMI088_ACC_ENABLE_ACC_ON, BMI088_ACC_PWR_CTRL_ERROR},
    {BMI088_ACC_PWR_CONF, BMI088_ACC_PWR_ACTIVE_MODE, BMI088_ACC_PWR_CONF_ERROR},
    {BMI088_ACC_CONF, BMI088_ACC_NORMAL | BMI088_ACC_800_HZ | BMI088_ACC_CONF_MUST_Set,
     BMI088_ACC_CONF_ERROR},
    {BMI088_ACC_RANGE, BMI088_ACC_RANGE_6G, BMI088_ACC_RANGE_ERROR},
    {BMI088_INT1_IO_CTRL,
     BMI088_ACC_INT1_IO_ENABLE | BMI088_ACC_INT1_GPIO_PP | BMI088_ACC_INT1_GPIO_LOW,
     BMI088_INT1_IO_CTRL_ERROR},
    {BMI088_INT_MAP_DATA, BMI088_ACC_INT1_DRDY_INTERRUPT, BMI088_INT_MAP_DATA_ERROR}};
// BMI088初始化配置数组for gyro,第一列为reg地址,第二列为写入的配置值,第三列为错误码(如果出错)
static uint8_t BMI088_Gyro_Init_Table[BMI088_WRITE_GYRO_REG_NUM][3] = {
    {BMI088_GYRO_RANGE, BMI088_GYRO_2000, BMI088_GYRO_RANGE_ERROR},
    {BMI088_GYRO_BANDWIDTH, BMI088_GYRO_1000_116_HZ | BMI088_GYRO_BANDWIDTH_MUST_Set,
     BMI088_GYRO_BANDWIDTH_ERROR},
    {BMI088_GYRO_LPM1, BMI088_GYRO_NORMAL_MODE, BMI088_GYRO_LPM1_ERROR},
    {BMI088_GYRO_CTRL, BMI088_DRDY_ON, BMI088_GYRO_CTRL_ERROR},
    {BMI088_GYRO_INT3_INT4_IO_CONF, BMI088_GYRO_INT3_GPIO_PP | BMI088_GYRO_INT3_GPIO_LOW,
     BMI088_GYRO_INT3_INT4_IO_CONF_ERROR},
    {BMI088_GYRO_INT3_INT4_IO_MAP, BMI088_GYRO_DRDY_IO_INT3, BMI088_GYRO_INT3_INT4_IO_MAP_ERROR}};

/* 内部使用的bmi088读写函数 */
static inline UINT _bmi088_writedata(SPI_Device *device, const uint8_t addr, const uint8_t *data,
                                     const uint8_t data_len, uint32_t timeout)
{
    UINT    status = TX_SUCCESS;
    uint8_t ptdata = addr & BMI088_SPI_WRITE_CODE;
    status         = BSP_SPI_TransAndTrans(device, &ptdata, 1, data, data_len, timeout);
    return status;
}

static inline UINT _bmi088_readdata(SPI_Device *device, const uint8_t addr, uint8_t *data,
                                    const uint8_t data_len, uint8_t isacc, uint32_t timeout)
{
    UINT status = TX_SUCCESS;

    static uint8_t tmp[8];
    static uint8_t tx[8] = {0};

    // data_len 最大为6，保证 tmp 不越界
    if (data_len > 6)
    {
        return TX_DELETED;
    }

    if (isacc)
    {
        tx[0]  = BMI088_SPI_READ_CODE | addr;
        status = BSP_SPI_TransReceive(device, tx, tmp, data_len + 2, timeout);
        memcpy(data, tmp + 2, data_len); // 前2字节dummy byte
        return status;
    }
    else
    {
        tx[0]  = BMI088_SPI_READ_CODE | addr;
        status = BSP_SPI_TransReceive(device, tx, tmp, data_len + 1, timeout);
        memcpy(data, tmp + 1, data_len); // 前1字节dummy byte
        return status;
    }
    return TX_DELETED;
}

/* acc gyro 初始化 */
/*加速度计部分*/
static inline void bmi088_acc_init()
{
    uint8_t whoami_check = 0;
    uint8_t tmp;
    // 加速度计以I2C模式启动,需要一次上升沿来切换到SPI模式,因此进行一次fake write
    _bmi088_readdata(bmi088_device.acc_device, BMI088_ACC_CHIP_ID, &whoami_check, 1, 1, TX_NO_WAIT);
    DWT_Delay(0.001);

    tmp = BMI088_ACC_SOFTRESET_VALUE;
    _bmi088_writedata(bmi088_device.acc_device, BMI088_ACC_SOFTRESET, &tmp, 1,
                      TX_NO_WAIT); // 软复位
    DWT_Delay(BMI088_COM_WAIT_SENSOR_TIME);

    _bmi088_readdata(bmi088_device.acc_device, BMI088_ACC_CHIP_ID, &whoami_check, 1, 1, TX_NO_WAIT);
    DWT_Delay(0.001);

    uint8_t             reg = 0, data = 0;
    BMI088_ERORR_CODE_e error = BMI088_NO_ERROR;
    for (uint8_t i = 0; i < sizeof(BMI088_Accel_Init_Table) / sizeof(BMI088_Accel_Init_Table[0]);
         i++)
    {
        reg  = BMI088_Accel_Init_Table[i][BMI088REG];
        data = BMI088_Accel_Init_Table[i][BMI088DATA];
        _bmi088_writedata(bmi088_device.acc_device, reg, &data, 1, TX_NO_WAIT); // 写入寄存器
        DWT_Delay(0.001);
        _bmi088_readdata(bmi088_device.acc_device, reg, &tmp, 1, 1,
                         TX_NO_WAIT); // 写完之后立刻读回检查
        DWT_Delay(0.001);
        if (tmp != BMI088_Accel_Init_Table[i][BMI088DATA])
        {
            error |= BMI088_Accel_Init_Table[i][BMI088ERROR];
            bmi088_device.BMI088_ERORR_CODE = error;
            log_e("ACCEL Init failed reg:%d", BMI088_Accel_Init_Table[i][BMI088REG]);
        }
    }
}

static inline void bmi088_gyro_init()
{
    uint8_t tmp;

    tmp = BMI088_GYRO_SOFTRESET_VALUE;
    _bmi088_writedata(bmi088_device.gyro_device, BMI088_GYRO_SOFTRESET, &tmp, 1,
                      TX_NO_WAIT); // 软复位
    DWT_Delay(BMI088_COM_WAIT_SENSOR_TIME);

    // 检查ID,如果不是0x0F(bmi088 whoami寄存器值),则返回错误
    uint8_t whoami_check = 0;
    _bmi088_readdata(bmi088_device.gyro_device, BMI088_GYRO_CHIP_ID, &whoami_check, 1, 0,
                     TX_NO_WAIT);
    if (whoami_check != BMI088_GYRO_CHIP_ID_VALUE)
    {
        log_e("No gyro Sensor!");
    }
    DWT_Delay(0.001);
    uint8_t             reg = 0, data = 0;
    BMI088_ERORR_CODE_e error = BMI088_NO_ERROR;
    for (uint8_t i = 0; i < sizeof(BMI088_Gyro_Init_Table) / sizeof(BMI088_Gyro_Init_Table[0]); i++)
    {
        reg  = BMI088_Gyro_Init_Table[i][BMI088REG];
        data = BMI088_Gyro_Init_Table[i][BMI088DATA];
        _bmi088_writedata(bmi088_device.gyro_device, reg, &data, 1, TX_NO_WAIT);
        DWT_Delay(0.001);
        _bmi088_readdata(bmi088_device.gyro_device, reg, &tmp, 1, 0,
                         TX_NO_WAIT); // 写完之后立刻读回对应寄存器检查是否写入成功
        DWT_Delay(0.001);
        if (tmp != BMI088_Gyro_Init_Table[i][BMI088DATA])
        {
            error |= BMI088_Gyro_Init_Table[i][BMI088ERROR];
            bmi088_device.BMI088_ERORR_CODE = error;
            log_e("GYRO Init failed reg:%d", BMI088_Gyro_Init_Table[i][BMI088REG]);
        }
    }
}

UINT Module_BMI088_get_accel()
{
    if (!bmi088_device.initialized)
    {
        return TX_DELETED;
    }
    UINT    status = TX_SUCCESS;
    uint8_t buf[8];
    // 读取accel的x轴数据首地址,bmi088内部自增读取地址 // 3* sizeof(int16_t)
    status =
        _bmi088_readdata(bmi088_device.acc_device, BMI088_ACCEL_XOUT_L, buf, 6, 1, TX_WAIT_FOREVER);
    for (uint8_t i = 0; i < 3; i++)
    {
        if (bmi088_device.BMI088_Cali_Offset.Calibrated)
        {
            bmi088_device.acc[i] = (BMI088_ACCEL_6G_SEN) *
                                   (float)((int16_t)((buf[2 * i + 1]) << 8) | buf[2 * i]) *
                                   bmi088_device.BMI088_Cali_Offset.AccelScale;
        }
        else
        {
            bmi088_device.acc[i] =
                (BMI088_ACCEL_6G_SEN) * (float)((int16_t)((buf[2 * i + 1]) << 8) | buf[2 * i]);
        }
    }
    return status;
}

UINT Module_BMI088_get_gyro()
{
    if (!bmi088_device.initialized)
    {
        return TX_DELETED;
    }
    UINT    status = TX_SUCCESS;
    uint8_t buf[8];
    status =
        _bmi088_readdata(bmi088_device.gyro_device, BMI088_GYRO_X_L, buf, 6, 0, TX_WAIT_FOREVER);
    if (bmi088_device.BMI088_Cali_Offset.Calibrated)
    {
        for (uint8_t i = 0; i < 3; i++)
        {
            bmi088_device.gyro[i] =
                BMI088_GYRO_2000_SEN * (float)((int16_t)((buf[2 * i + 1]) << 8) | buf[2 * i]) -
                bmi088_device.BMI088_Cali_Offset.GyroOffset[i];
        }
    }
    else
    {
        for (uint8_t i = 0; i < 3; i++)
        {
            bmi088_device.gyro[i] =
                BMI088_GYRO_2000_SEN * (float)((int16_t)((buf[2 * i + 1]) << 8) | buf[2 * i]);
        }
    }

    return status;
}

UINT Module_BMI088_get_temp()
{
    if (!bmi088_device.initialized)
    {
        return TX_DELETED;
    }

    UINT    status = TX_SUCCESS;
    uint8_t buf[8];
    status = _bmi088_readdata(bmi088_device.acc_device, BMI088_TEMP_M, buf, 2, 1, TX_WAIT_FOREVER);
    int16_t tmp = (((buf[0] << 3) | (buf[1] >> 5)));
    if (tmp > 1023)
    {
        tmp -= 2048;
    }
    bmi088_device.temperature = (float)(int16_t)tmp * BMI088_TEMP_FACTOR + BMI088_TEMP_OFFSET;

    return status;
}

void Module_BMI088_temp_ctrl()
{
#if BMI088_TEMP_ENABLE
    PIDCalculate(&bmi088_device.pid_temp, bmi088_device.temperature, BMI088_TEMP_SET);
    // 使用BSP_PWM设置占空比
    if (bmi088_device.bmi088_pwm)
    {
        // 将PID输出限制在0-1000范围内
        float duty_cycle = bmi088_device.pid_temp.Output;
        if (duty_cycle < 0)
        {
            duty_cycle = 0;
        }
        else if (duty_cycle > 1000)
        {
            duty_cycle = 1000;
        }
        BSP_PWM_SetDutyCycle(bmi088_device.bmi088_pwm, (uint16_t)duty_cycle);
    }
#endif
}

Bmi088_device_t *Module_BMI088_get_device()
{
    if (!bmi088_device.initialized)
    {
        return NULL;
    }
    else
    {
        return &bmi088_device;
    }
}

UINT Module_BMI088_init()
{

bmi088_device.initialized = 0;

#if defined(STM32H723xx)
    SPI_Device_Init_Config gyro_cfg = {
        .hspi    = &hspi2,
        .cs_port = GPIOC,
        .cs_pin  = GPIO_PIN_3,
        .tx_mode = SPI_MODE_BLOCKING,
        .rx_mode = SPI_MODE_BLOCKING,
    };
    bmi088_device.gyro_device      = BSP_SPI_Device_Init(&gyro_cfg);
    SPI_Device_Init_Config acc_cfg = {
        .hspi    = &hspi2,
        .cs_port = GPIOC,
        .cs_pin  = GPIO_PIN_0,
        .tx_mode = SPI_MODE_BLOCKING,
        .rx_mode = SPI_MODE_BLOCKING,
    };
    bmi088_device.acc_device = BSP_SPI_Device_Init(&acc_cfg);
    PWM_Init_Config config   = {
          .channel = TIM_CHANNEL_4, .dutyx10 = 0, .htim = &htim3, .Mode = PWM_MODE_BLOCKING};
    bmi088_device.bmi088_pwm = BSP_PWM_Device_Init(&config);
#elif defined(STM32F407xx)
    SPI_Device_Init_Config gyro_cfg = {
        .hspi    = &hspi1,
        .cs_port = GPIOB,
        .cs_pin  = GPIO_PIN_0,
        .tx_mode = SPI_MODE_BLOCKING,
        .rx_mode = SPI_MODE_BLOCKING,
    };
    bmi088_device.gyro_device      = BSP_SPI_Device_Init(&gyro_cfg);
    SPI_Device_Init_Config acc_cfg = {
        .hspi    = &hspi1,
        .cs_port = GPIOA,
        .cs_pin  = GPIO_PIN_4,
        .tx_mode = SPI_MODE_BLOCKING,
        .rx_mode = SPI_MODE_BLOCKING,
    };
    bmi088_device.acc_device      = BSP_SPI_Device_Init(&acc_cfg);
    static PWM_Init_Config config = {
        .channel = TIM_CHANNEL_1, .dutyx10 = 0, .htim = &htim10, .Mode = PWM_MODE_BLOCKING};
    bmi088_device.bmi088_pwm = BSP_PWM_Device_Init(&config);
#endif
    if (!bmi088_device.acc_device || !bmi088_device.gyro_device || !bmi088_device.bmi088_pwm)
    {
        log_e("bmi088 init failed");
        return TX_DELETED;
    }
    else
    {
        bmi088_acc_init();
        bmi088_gyro_init();
        PID_Init_Config_s config = {.MaxOut        = 1000,
                                    .IntegralLimit = 20,
                                    .DeadBand      = 0,
                                    .Kp            = 20,
                                    .Ki            = 0,
                                    .Kd            = 0,
                                    .Improve       = 0x01};
        PIDInit(&bmi088_device.pid_temp, &config);
    }

#if BMI088_TEMP_ENABLE
    BSP_PWM_Start(bmi088_device.bmi088_pwm, NULL, 0);
#endif
    bmi088_device.initialized = 1;

    static uint8_t tmpdata[sizeof(BMI088_Cali_Offset_t) + 2] = {0};

#if defined(STM32H723xx)
    Module_W25qxx_init();
    Module_W25Qxx_ReadBuffer(tmpdata, sizeof(tmpdata));
#elif defined(STM32F407xx)
    BSP_FLASH_Read_Buffer(tmpdata, sizeof(tmpdata));
#endif

    if (tmpdata[sizeof(BMI088_Cali_Offset_t) + 1] != 0xAA)
    {
        Module_BMI088_calibrate_bmi088_offset(&bmi088_device);
    }
    else
    {
        memcpy(&bmi088_device.BMI088_Cali_Offset, tmpdata, sizeof(BMI088_Cali_Offset_t));
    }
    if (bmi088_device.BMI088_ERORR_CODE == BMI088_NO_ERROR)
    {
        
        log_i("bmi088 init success");
        return TX_SUCCESS;
    }
    return TX_DELETED;
}
