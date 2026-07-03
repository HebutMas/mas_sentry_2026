# 创建静态库
set(name BSP)

# 根据芯片类型选择对应的BSP驱动文件
if(DEFINED TARGET_FAMILY)
    if(${TARGET_FAMILY} STREQUAL "F4")
        add_library(${name} STATIC
            bsp_init.c
            GPIO/bsp_gpio.c
            UART/bsp_uart_f4.c
            DWT/bsp_dwt.c
            SPI/bsp_spi_f4.c
            PWM/bsp_pwm.c
            I2C/bsp_i2c_f4.c
            USB/bsp_cdc_acm.c
            CAN/bsp_can_f4.c
            FLASH/bsp_flash.c
        )
        message("Using STM32F4 BSP driver")
    elseif(${TARGET_FAMILY} STREQUAL "H7")
        add_library(${name} STATIC
            bsp_init.c
            GPIO/bsp_gpio.c
            UART/bsp_uart_h7.c
            DWT/bsp_dwt.c
            SPI/bsp_spi_h7.c
            PWM/bsp_pwm.c
            USB/bsp_cdc_acm.c
            CAN/bsp_can_h7.c
        )
        message("Using STM32H7 BSP driver")
    endif()
else()
    message(ERROR "TARGET_FAMILY not defined")

endif()

# 设置包含目录
target_include_directories(${name}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/UART
    ${CMAKE_CURRENT_SOURCE_DIR}/DWT
    ${CMAKE_CURRENT_SOURCE_DIR}/SPI
    ${CMAKE_CURRENT_SOURCE_DIR}/PWM
    ${CMAKE_CURRENT_SOURCE_DIR}/I2C
    ${CMAKE_CURRENT_SOURCE_DIR}/GPIO
    ${CMAKE_CURRENT_SOURCE_DIR}/USB
    ${CMAKE_CURRENT_SOURCE_DIR}/CAN
    ${CMAKE_CURRENT_SOURCE_DIR}/FLASH
)

# 链接必要的库
target_link_libraries(${name} stm32cubemx TOOLS MODULES)

# 添加到主项目
target_link_libraries(${PROJECT_NAME} ${name})
