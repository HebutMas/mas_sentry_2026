# 创建静态库
set(name ROBOT_INIT)


add_library(${name} STATIC
    robot_init.c
)

# 设置包含目录
target_include_directories(${name}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接必要的库
target_link_libraries(${name} stm32cubemx TOOLS BSP MODULES APPS)

# 添加到主项目
target_link_libraries(${PROJECT_NAME} ${name})