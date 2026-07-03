# 创建静态库
set(name MODULES)

add_library(${name} STATIC
    RGB/module_rgb.c
    BEEP/module_beep.c
    OFFLINE/module_offline.c
    W25Q64/module_w25q64.c
    algorithm/pid.c
    algorithm/user_lib.c
    algorithm/crc_rm.c
    algorithm/lqr.c
    BMI088/module_bmi088.c
    BMI088/module_bmi088_cali.c
    INS/module_ins.c
    INS/kalman_filter.c
    INS/QuaternionEKF.c
    REMOTE/DT7/dt7.c
    REMOTE/SBUS/sbus.c
    REMOTE/VT02/vt02.c
    REMOTE/VT03/vt03.c
    MOTOR/motor_register.c
    MOTOR/DJI/motor_dji.c
    MOTOR/DAMIAO/motor_damiao.c
    MOTOR/DJI/POWER_CONTROL/power_control.c
    BOARD_COMM/module_board_comm.c
    ITC/module_itc.c
    WT606/module_wt606.c
    REFEREE/referee_protocol.c
    REFEREE/referee_protocol_ui.c
    REFEREE/referee_user_ui.c
    SuperCap/module_supercap.c
)

# 设置包含目录
target_include_directories(${name}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/RGB
    ${CMAKE_CURRENT_SOURCE_DIR}/BEEP
    ${CMAKE_CURRENT_SOURCE_DIR}/OFFLINE
    ${CMAKE_CURRENT_SOURCE_DIR}/W25Q64
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/BMI088
    ${CMAKE_CURRENT_SOURCE_DIR}/INS
    ${CMAKE_CURRENT_SOURCE_DIR}/REMOTE
    ${CMAKE_CURRENT_SOURCE_DIR}/REMOTE/DT7
    ${CMAKE_CURRENT_SOURCE_DIR}/REMOTE/SBUS
    ${CMAKE_CURRENT_SOURCE_DIR}/REMOTE/VT02
    ${CMAKE_CURRENT_SOURCE_DIR}/REMOTE/VT03
    ${CMAKE_CURRENT_SOURCE_DIR}/MOTOR
    ${CMAKE_CURRENT_SOURCE_DIR}/MOTOR/DJI
    ${CMAKE_CURRENT_SOURCE_DIR}/MOTOR/DAMIAO
    ${CMAKE_CURRENT_SOURCE_DIR}/MOTOR/DJI/POWER_CONTROL
    ${CMAKE_CURRENT_SOURCE_DIR}/BOARD_COMM
    ${CMAKE_CURRENT_SOURCE_DIR}/ITC
    ${CMAKE_CURRENT_SOURCE_DIR}/WT606
    ${CMAKE_CURRENT_SOURCE_DIR}/REFEREE
    ${CMAKE_CURRENT_SOURCE_DIR}/SuperCap
)

# 链接必要的库
target_link_libraries(${name} stm32cubemx BSP TOOLS CMSISDSP CONFIG)
target_link_libraries(MODULES m) #链接数学库

# 添加到主项目
target_link_libraries(${PROJECT_NAME} ${name})