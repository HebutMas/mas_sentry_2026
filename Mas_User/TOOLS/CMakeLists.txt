# 创建静态库
set(name TOOLS)

enable_language(C ASM)

add_library(${name} STATIC
    tools_init.c
    LOG/tool_log.c
    RTT/SEGGER_RTT.c
    RTT/SEGGER_RTT_printf.c
    RTT/SEGGER_RTT_ASM_ARMv7M.S
    CherryUSB/core/usbd_core.c 
    CherryUSB/class/cdc/usbd_cdc_acm.c 
    CherryUSB/port/dwc2/usb_dc_dwc2.c 
    CherryUSB/port/dwc2/usb_glue_st.c
)

# 设置包含目录
target_include_directories(${name}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/RTT
    ${CMAKE_CURRENT_SOURCE_DIR}/LOG
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/core
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/cdc
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/common 
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/port/dwc2 
    ${CMAKE_CURRENT_SOURCE_DIR}/CherryUSB/class/hub
)

# 链接必要的库
target_link_libraries(${name} stm32cubemx BSP)

# 添加到主项目
target_link_libraries(${PROJECT_NAME} ${name})
