#include "bsp_cdc_acm.h"
#include "tx_api.h"
#include "usbd_core.h"
#include "usbd_cdc_acm.h"

#include "module_offline.h"

#define LOG_TAG "usb"
#define LOG_LVL LOG_LVL_DBG
#include "tool_log.h"

/*============================================================================
 * 配置定义
 *===========================================================================*/

#define CDC_IN_EP          0x81
#define CDC_OUT_EP         0x01
#define CDC_INT_EP         0x82

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

/*============================================================================
 * USB 描述符
 *===========================================================================*/

static const uint8_t device_descriptor[] = {USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01)};

static const uint8_t config_descriptor[] = {USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
                                            CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, CDC_MAX_MPS, 0x02)};

static const uint8_t device_quality_descriptor[] = {0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00};

static const char *string_descriptors[] = {(const char[]){0x09, 0x04}, "CherryUSB", "CherryUSB CDC DEMO", "2022123456"};

static const uint8_t *device_descriptor_callback(uint8_t speed) { return device_descriptor; }
static const uint8_t *config_descriptor_callback(uint8_t speed) { return config_descriptor; }
static const uint8_t *device_quality_descriptor_callback(uint8_t speed) { return device_quality_descriptor; }
static const char    *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index >= 4) return NULL;
    return string_descriptors[index];
}

static const struct usb_descriptor cdc_descriptor = {.device_descriptor_callback         = device_descriptor_callback,
                                                     .config_descriptor_callback         = config_descriptor_callback,
                                                     .device_quality_descriptor_callback = device_quality_descriptor_callback,
                                                     .string_descriptor_callback         = string_descriptor_callback};

/*============================================================================
 * 内部变量
 *===========================================================================*/

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_buf[2][CDC_MAX_MPS];
static uint8_t                                 active_idx = 0;
static TX_SEMAPHORE                            usb_tx_sem;
static OfflineDevice                          *offline_usb_dev = NULL;
static ReceivePacket                           receive;

/*============================================================================
 * 回调函数
 *===========================================================================*/

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event)
    {
    case USBD_EVENT_RESET:
        tx_semaphore_put(&usb_tx_sem);
        break;
    case USBD_EVENT_CONFIGURED:
        usbd_ep_start_read(busid, CDC_OUT_EP, read_buf[active_idx], CDC_MAX_MPS);
        tx_semaphore_put(&usb_tx_sem);
        break;
    default:
        break;
    }
}

/* 接收回调 */
void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    active_idx ^= 1;

    // 在接收中断中直接处理数据验证
    if (nbytes == sizeof(ReceivePacket))
    {
        uint8_t  read_idx = !active_idx;
        uint8_t *buf      = read_buf[read_idx];

        if (buf[0] == 0XBB && buf[sizeof(ReceivePacket) - 1] == 0X5B)
        {
            memcpy(&receive, buf, sizeof(ReceivePacket));
            Module_Offline_device_update(offline_usb_dev);
        }
    }

    usbd_ep_start_read(busid, CDC_OUT_EP, read_buf[active_idx], CDC_MAX_MPS);
}

/* 发送完成回调 */
void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    // 判断是否需要发送 ZLP (Zero Length Packet)
    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes)
    {
        // 如果需要发送 ZLP，此时任务继续等待，直到 ZLP 发送完成
        usbd_ep_start_write(busid, CDC_IN_EP, NULL, 0);
    }
    else
    {
        // 数据（或 ZLP）发送完毕，释放信号量
        tx_semaphore_put(&usb_tx_sem);
    }
}

static struct usbd_endpoint cdc_out_ep = {.ep_addr = CDC_OUT_EP, .ep_cb = usbd_cdc_acm_bulk_out};
static struct usbd_endpoint cdc_in_ep  = {.ep_addr = CDC_IN_EP, .ep_cb = usbd_cdc_acm_bulk_in};

/*============================================================================
 * 接口函数
 *===========================================================================*/

static struct usbd_interface intf0;
static struct usbd_interface intf1;

void cdc_acm_init(uint8_t busid, uintptr_t reg_base)
{
    tx_semaphore_create(&usb_tx_sem, "usb_tx_sem", 1);

    usbd_desc_register(busid, &cdc_descriptor);
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf0));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &intf1));
    usbd_add_endpoint(busid, &cdc_out_ep);
    usbd_add_endpoint(busid, &cdc_in_ep);
    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void cdc_acm_data_send(SendPacket *sendpacket, uint32_t timeout)
{
    tx_semaphore_get(&usb_tx_sem, timeout);
    usbd_ep_start_write(0, CDC_IN_EP, (uint8_t *)sendpacket, sizeof(SendPacket));
}

void cdc_acm_offline_register(void)
{
    Offline_Init_config_t config = {.name = "minipc", .beep_times = 0, .enable = OFFLINE_ENABLE, .timeout_ms = 100};
    offline_usb_dev              = Module_Offline_device_register(&config);
    if (offline_usb_dev == NULL)
    {
        log_e("offline device register error");
        return;
    }
}

ReceivePacket *cdc_acm_read_data()
{
    if (Module_Offline_get_device_status(offline_usb_dev) == STATE_OFFLINE)
    {
        return NULL;
    }
    return &receive;
}

uint8_t cdc_acm_usb_offline_state(void) { return Module_Offline_get_device_status(offline_usb_dev); }
