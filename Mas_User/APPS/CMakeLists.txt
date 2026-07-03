# 创建静态库
set(name APPS)


add_library(${name} STATIC
    app_init.c
    offline_task/offline_task.c
    ins_task/ins_task.c
    remote_task/remote_task.c
    motor_task/motor_task.c
    wt606_task/wt606_task.c
    robot_control/robot_control_fun.c
    robot_control/robot_control_task.c
    gimbal_task/gimbal_task.c
    shoot_task/shoot_task.c
    chassis_task/chassis_type.c 
    chassis_task/chassis_task.c
    referee_task/referee_task.c
    board_comm_task/board_comm_task.c
    supercap_task/supercap_task.c
)

# 设置包含目录
target_include_directories(${name}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_task
    ${CMAKE_CURRENT_SOURCE_DIR}/ins_task
    ${CMAKE_CURRENT_SOURCE_DIR}/remote_task
    ${CMAKE_CURRENT_SOURCE_DIR}/motor_task
    ${CMAKE_CURRENT_SOURCE_DIR}/wt606_task
    ${CMAKE_CURRENT_SOURCE_DIR}/robot_control
    ${CMAKE_CURRENT_SOURCE_DIR}/gimbal_task
    ${CMAKE_CURRENT_SOURCE_DIR}/shoot_task
    ${CMAKE_CURRENT_SOURCE_DIR}/chassis_task
    ${CMAKE_CURRENT_SOURCE_DIR}/referee_task
    ${CMAKE_CURRENT_SOURCE_DIR}/board_comm_task
    ${CMAKE_CURRENT_SOURCE_DIR}/supercap_task
)

# 链接必要的库
target_link_libraries(${name} stm32cubemx TOOLS BSP MODULES)

# 添加到主项目
target_link_libraries(${PROJECT_NAME} ${name})