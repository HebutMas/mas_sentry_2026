<!--
 * @Author: Mas sji733055@gmail.com
 * @Date: 2026-02-09 19:11:50
 * @LastEditors: Mas sji733055@gmail.com
 * @LastEditTime: 2026-02-13 17:02:54
 * @FilePath: \mas_rm_djic\Mas_User\MODULES\BEEP\BEEP.MD
 * @Description: 
-->
# BEEP 模块文档 

## 1. 概述 
STM32H7/F4通用蜂鸣器模块。

## 2. 架构设计
### 2.1 代码架构
```c

公开API层：
├─ Module_Beep_init()                    // 初始化 BEEP 设备
├─ Module_Beep_Start()              //启动BEEP所属PWM定时器
├─ Module_Beep_Stop()               //关闭BEEP所属PWM定时器
└─ Module_Beep_set()               // 修改PWM的分频与脉冲宽度
```
## 3. API接口
```c  
void Module_Beep_init();
```
```c
void Module_Beep_Start();
```
```c
void Module_Beep_Stop();
```
```c
void Module_Beep_set(uint16_t psc, uint16_t pwm);
```
## 3. 注意事项 
1.  使用前注意所用主控芯片为STM32H723xx还是STM32F407xx，改动需改变宏定义
2.  本模块强依赖 BSP_PWM (蜂鸣调节)
## 4.常见问题排查
- 问题；蜂鸣器无响应。
        - 排查：1. 宏定义是否改动为主控
                2. 前往掉线检测模块检测错误
## 5.使用示例
```c
Module_Beep_init();//初始化蜂鸣器
Module_Beep_Start();//初始化后默认关闭蜂鸣器，需先启动蜂鸣器

//以掉线检测模块为例，启动蜂鸣器后需使用下面的函数设置分频与脉宽使蜂鸣器产生蜂鸣
Module_Beep_set(OFFLINE_BEEP_TUNE_VALUE, OFFLINE_BEEP_CTRL_VALUE);
//后通过设置为0，产生周期性蜂鸣通过蜂鸣次数判断优先级
Module_Beep_set(0, 0);
```    

## 6. 版本历史 

### v1.0 (2026-02-05) 
    - 初始版本
               
