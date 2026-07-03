# BSP_USB 驱动文档 

## 1. 概述 
基于 CherryUSB 的 USB CDC ACM 虚拟串口驱动，适用于 STM32H7/F4 系列。

## 2. 架构设计
### 代码架构
```c
内部辅助/回调层：
├─ usbd_event_handler()              // USB 事件处理 (Reset/Configured)
├─ usbd_cdc_acm_bulk_out()          // 接收完成 ISR
└─ usbd_cdc_acm_bulk_in()           // 发送完成 ISR

公开API层：
├─ cdc_acm_init()                    // 初始化 USB 设备
├─ cdc_acm_data_send()              // 阻塞发送数据
└─ cdc_acm_read_data()              // 读取最新数据
```

## 3. API接口
```c
void cdc_acm_init(uint8_t busid, uintptr_t reg_base);
```
```c
UINT cdc_acm_data_send(uint8_t *data, uint32_t len, uint32_t timeout);
```
```c
uint8_t* cdc_acm_read_data(uint8_t *len);
```
## 4. 双缓冲机制
```
数据流向：
ISR 接收端          应用读取端
     ↓                    ↑
[ read_buf[0] ]      ← read_idx (!active_idx)
  (当前正在写)            (返回给应用)
     ↓                    ↑
[ read_buf[1] ]      ← (上一次写好的数据)

切换逻辑 (ISR):
1. 数据写入 read_buf[active_idx] 完成
2. active_idx ^= 1 (切换索引，将此 buffer 交给应用，ISR 拿新的 buffer)
3. usbd_ep_start_read(新的 buffer) (立即开始接收下一包，覆盖旧 buffer)
```
## 5. 注意事项 
1. STM32H7 缓存: 
    - 缓冲区必须放在 Non-Cacheable RAM (DTCM/RAM_D2/RAM_D3)
2. 基址与初始化顺序:（stm32特性 hs/fs只看引脚，与cubemx配置无关） 
    - config里的 CONFIG_USB_HS 针对引脚（pa14,pa15）才能选，对于（pa11，pa12）不用声明

## 6. 版本历史 

### v1.0 (2026-01-17) 
    - 初始版本
               
