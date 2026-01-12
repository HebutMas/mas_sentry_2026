<!--
 * @Author: laladuduqq 2807523947@qq.com
 * @Date: 2026-01-12 23:22:56
 * @LastEditors: laladuduqq 2807523947@qq.com
 * @LastEditTime: 2026-01-12 23:24:20
 * @FilePath: /MAS_RM_DAMIAOH7/README.MD
 * @Description: 
-->
# 项目简介
山海机甲达妙H7开发板开发框架
# 开发环境配置
[开发环境配置](doc/开发环境搭建.MD)
# 编译项目

```bash
# 克隆项目
git clone --recursive git@gitee.com:laladuduqq/mas_rm_dmh7.git (ssh)
gut clone --recursive https://gitee.com/laladuduqq/mas_rm_dmh7.git (https)
# 创建构建目录
mkdir build && cd build
# 配置CMake（Debug构建）
cmake -DCMAKE_BUILD_TYPE=Debug ..
# 编译项目
cmake --build . -j$(nproc)
# 或使用make
make -j$(nproc)
```

# 烧写固件

```
执行task.json的任务，根据daplink stlink jlink 区分
```

# 📋 TODO清单
- [ ] BSP层开发测试
    - [ ] uart(polling/it/dma)
    - [ ] spi(polling/it/dma)
    - [ ] i2c(polling/it)
    - [ ] adc(polling/it/dma)
    - [ ] gpio_exti(it)
    - [ ] can(polling/it)
    - [ ] pwm(polling/it/dma)
    - [ ] usb(it/dma) -- tinyusb
# 🤝 贡献指南

在提交代码前，请确保：

1. 代码风格符合项目规范（使用 `.clang-format`）
2. 添加必要的注释和文档
3. 编译不产生警告
4. 相关模块已充分测试