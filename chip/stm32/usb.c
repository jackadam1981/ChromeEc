/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "debug.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USB, outstr)
#define CPRINTF(format, args...) /*debug_printf(format, ## args)*/

#define MAX_PACKET_SIZE 64

#define N_ENDPOINTS 3

#define HID_REPORT_SIZE  8

#define USB_EP_SERIAL_TX 1
#define USB_EP_SERIAL_RX 2

#define USB_CONFIG_SIZE (USB_DT_CONFIG_SIZE + \
			 USB_DT_INTERFACE_SIZE + \
			 USB_DT_ENDPOINT_SIZE * 2)

/* USB Standard Device Descriptor */
static const struct usb_device_descriptor dev_desc = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200, /* v2.00 */
	.bDeviceClass = USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass = 0x00,
	.bDeviceProtocol =0x00,
	.bMaxPacketSize0 = MAX_PACKET_SIZE,
	.idVendor = USB_VID_GOOGLE,
	.idProduct = CONFIG_USB_PID,
	.bcdDevice = 0x0200, /* 2.00 */
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1
};

/* USB Configuration Descriptor */
const struct usb_config_descriptor USB_CONF_DESC(conf) = {
	.bLength = USB_DT_CONFIG_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = USB_CONFIG_SIZE, /* no of returned bytes */
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80, /* bus powered */
	.bMaxPower = 250, /* MaxPower 500 mA */
};
/* USB-Serial descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_SERIAL) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_SERIAL, 81) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x80 | USB_EP_SERIAL_TX,
	.bmAttributes = 0x02 /* Bulk IN */,
	.wMaxPacketSize = MAX_PACKET_SIZE,
	.bInterval = 10
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_SERIAL, 2) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_EP_SERIAL_RX,
	.bmAttributes = 0x02 /* Bulk OUT */,
	.wMaxPacketSize = MAX_PACKET_SIZE,
	.bInterval = 0
};

const uint8_t usb_string_desc[] = {
	4, /* Descriptor size */
	USB_DT_STRING,
	0x09, 0x04 /* LangID = 0x0409: U.S. English */
};

const void * const usb_strings[] = {(const void *)usb_string_desc,
				    USB_STRING_DESC("Google Inc."),
				    USB_STRING_DESC("Keyborg"),
				    USB_STRING_DESC("3")};
#define USB_STR_COUNT sizeof(usb_strings)

/* USB SRAM addressing */
#ifdef CHIP_FAMILY_STM32F0
typedef uint16_t usb_uint;
#else
/* older chips use a weird addressing were 16-bit words are 32-bit appart */
typedef uint32_t usb_uint;
#endif

union endpoint {
	struct {
		usb_uint tx_addr;
		usb_uint tx_count;
		usb_uint rx_addr;
		usb_uint rx_count;
	} sb; /* Single buffered */
	struct {
		usb_uint addr;
		usb_uint count;
	} db_tx[2]; /* TX double buffered */
	struct {
		usb_uint addr;
		usb_uint count;
	} db_rx[2]; /* RX double buffered */
};

static struct usb_sram {
	union endpoint ep[N_ENDPOINTS];
	usb_uint ep0_tx[MAX_PACKET_SIZE / 2];
	usb_uint ep0_rx[MAX_PACKET_SIZE / 2];
	usb_uint ep1_tx[2][MAX_PACKET_SIZE / 2];
	usb_uint ep2_rx[MAX_PACKET_SIZE / 2];
} *desc = (void *)STM32_USB_CAN_SRAM_BASE;

/* Compute the address inside SRAM for the USB controller */
#define usb_sram_addr(x) \
	(offsetof(struct usb_sram, x) / (sizeof(usb_uint)/sizeof(uint16_t)))

static void copy_to_endpoint(const uint8_t *src, usb_uint *ebuf, int size)
{
	int i;

	for (i = 0; i < size / 2; i++, src += 2)
		*ebuf++ = src[0] | (src[1] << 8);
	if (size & 1)
		*ebuf++ = src[size - 1];
}

#define EP_MASK     0x0F0F
#define EP_TX_MASK  0x0030
#define EP_TX_VALID 0x0030
#define EP_TX_NAK   0x0020
#define EP_TX_STALL 0x0010
#define EP_TX_DISAB 0x0000
#define EP_RX_MASK  0x3000
#define EP_RX_VALID 0x3000
#define EP_RX_NAK   0x2000
#define EP_RX_STALL 0x1000
#define EP_RX_DISAB 0x0000

#define TOGGLE_EP(n, mask, val, flags) \
	STM32_USB_EP(n) = (((STM32_USB_EP(n) & (EP_MASK | (mask))) \
			   ^ (val)) | (flags))

static inline int get_sw_buf(void)
{
	return !!(STM32_USB_EP(1) & (1 << 14));
}

static inline void toggle_sw_buf(void)
{
	TOGGLE_EP(1, 0x0, 0x0, 1 << 14);
}

static inline void set_sw_buf(int x)
{
	if (x != get_sw_buf())
		toggle_sw_buf();
}

static inline int get_dtog(void)
{
	return !!(STM32_USB_EP(1) & (1 << 6));
}

static int set_addr;

static void ep0_rx(void)
{
	/* TODO check setup bit ? */
	if ((desc->ep0_rx[0] == 0x0680) &&
		(desc->ep0_rx[1] == 0x0100)) {
		/* Setup : Get device descriptor */
		copy_to_endpoint((void *)&dev_desc, desc->ep0_tx,
				 sizeof(dev_desc));
		desc->ep[0].sb.tx_count =  sizeof(dev_desc);
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_VALID | EP_RX_VALID,
			     1 << 8 /* STATUS OUT */);
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("DEVD %04x\n", STM32_USB_EP(0));
	} else if ((desc->ep0_rx[0] == 0x0680) &&
		(desc->ep0_rx[1] == 0x0200)) {
		/* Setup : Get configuration descriptor */
		copy_to_endpoint(__usb_desc, desc->ep0_tx, USB_CONFIG_SIZE);
		desc->ep[0].sb.tx_count = MIN(desc->ep0_rx[3], USB_CONFIG_SIZE);
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_VALID | EP_RX_VALID,
			     1 << 8 /* STATUS OUT */);
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("CFG %04x[l %04x]\n", STM32_USB_EP(0),
			desc->ep0_rx[3]);
	} else if ((desc->ep0_rx[0] == 0x0680) &&
		((desc->ep0_rx[1] >> 8) == 0x03)) {
		/* Setup : Get string descriptor */
		uint8_t idx = desc->ep0_rx[1] & 0xff;
		const uint8_t *str_desc;
		if (idx >= USB_STR_COUNT) { /* The string does not exist */
			TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
				     EP_TX_STALL | EP_RX_VALID,
				     0);
			return; /* dont remove the STALL */
		}
		str_desc = usb_strings[idx];
		copy_to_endpoint(str_desc, desc->ep0_tx, str_desc[0]);
		desc->ep[0].sb.tx_count = MIN(desc->ep0_rx[3], str_desc[0]);
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_VALID | EP_RX_VALID,
			     1 << 8 /* STATUS OUT */);
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("STR/%d %04x[l %04x]\n", idx, STM32_USB_EP(0),
			desc->ep0_rx[3]);
	} else if ((desc->ep0_rx[0] == 0x0680) &&
		(desc->ep0_rx[1] == 0x0600)) {
		/* Setup : Get device qualifier descriptor */
		/* Not high speed : STALL next IN used as handshake */
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_STALL | EP_RX_VALID,
			     0);
		CPRINTF("DEVQ %04x\n", STM32_USB_EP(0));
		return; /* dont remove the STALL */
	} else if ((desc->ep0_rx[0] == 0x0080) &&
		(desc->ep0_rx[1] == 0x0000)) {
		uint16_t zero = 0;
		/* Get status */
		copy_to_endpoint((void *)&zero, desc->ep0_tx, 2);
		desc->ep[0].sb.tx_count = 2;
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_VALID | EP_RX_VALID,
			     1 << 8 /* STATUS OUT */);
	} else if (desc->ep0_rx[0] == 0x0900) {
		/* SET_CONFIGURATION */
		/*uint8_t cfg = desc->ep0_rx[1] & 0xff;*/
		CPRINTF("SetCFG %d\n", cfg);
		/* null IN for handshake */
		desc->ep[0].sb.tx_count = 0;
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_VALID | EP_RX_VALID,
			     0);
	} else if (desc->ep0_rx[0] == 0x0500) {
		/* SET_ADDRESS */
		uint8_t addr = desc->ep0_rx[1] & 0xff;

		/* STM32_USB_DADDR = addr | 0x80; */
		/* set the address after we got IN packet handshake */
		set_addr = addr;
		CPRINTF("ADDR %02x/%d\n", addr, addr);
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_VALID | EP_RX_VALID,
			     0);
		/* need null IN transaction -> TX Valid */
	} else {
		/* Unknown descriptor ... */
		TOGGLE_EP(0, EP_TX_MASK | EP_RX_MASK,
			     EP_TX_STALL | EP_RX_VALID,
			     0);
		CPRINTF("CTR EP0 %04x\n", STM32_USB_EP(0));
	}
}

static void ep0_tx(void)
{
	if (set_addr) {
		STM32_USB_DADDR = set_addr | 0x80;
		set_addr = 0;
		CPRINTF("SETAD %02x\n", STM32_USB_DADDR);
	}

	TOGGLE_EP(0, EP_TX_MASK, EP_TX_VALID, 0);
	CPRINTF("CTR EP0 %04x\n", STM32_USB_EP(0));
}

void console_handle_char(int c);
static void ep2_rx(void)
{
	int size = desc->ep[2].sb.rx_count & 0x3ff;
	int i;

	for (i = 0; i < size / 2; i++) {
		console_handle_char(desc->ep2_rx[i] & 0xff);
		console_handle_char((desc->ep2_rx[i] >> 8) & 0xff);
	}
	if (size & 1)
		console_handle_char(desc->ep2_rx[size / 2] & 0xff);

	/* clear IT */
	TOGGLE_EP(2, EP_RX_MASK, EP_RX_VALID, 0);
	return;
}

int usb_buffer_available(void)
{
	if (desc->ep[1].db_tx[get_sw_buf()].count)
		return 0;
	return 1;
}

int usb_write_raw(const uint8_t *data, int size)
{
	int cbuf = get_sw_buf();

	while (!usb_buffer_available())
		;

	copy_to_endpoint(data, desc->ep1_tx[cbuf], size);
	desc->ep[1].db_tx[cbuf].count = size;
	/* enable TX */
	toggle_sw_buf();
	TOGGLE_EP(1, EP_TX_MASK, EP_TX_VALID, 0);
	/* Successful if we consumed all output */
	return (size > 64) ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

static void (* const ep_callback[32])(void) =
{
	/* TX endpoints */
	[0] = ep0_tx,
	[1] = NULL, /* Handled by high priority IRQ handler */
	[2] = NULL, /* RX only */
	/* RX endpoints */
	[16 + 0] = ep0_rx,
	[16 + 1] = NULL, /* TX only */
	[16 + 2] = ep2_rx,
};

void usb_reset(void)
{
	STM32_USB_EP(0) = (1 << 9) /* control EP */ |
			  (2 << 4) /* TX NAK */ |
			  (3 << 12) /* RX VALID */;

	desc->ep[0].sb.tx_addr = usb_sram_addr(ep0_tx);
	desc->ep[0].sb.rx_addr = usb_sram_addr(ep0_rx);
	desc->ep[0].sb.rx_count = 0x8000 | ((MAX_PACKET_SIZE/32-1) << 10);
	desc->ep[0].sb.tx_count = 0;

	/* Serial Bulk IN endpoint 1 */
	desc->ep[1].db_tx[0].addr = usb_sram_addr(ep1_tx[0]);
	desc->ep[1].db_tx[0].count = 0;
	desc->ep[1].db_tx[1].addr = usb_sram_addr(ep1_tx[1]);
	desc->ep[1].db_tx[1].count = 0;
	STM32_USB_EP(1) = (USB_EP_SERIAL_TX << 0) /*Endpoint Address: 1 */ |
			  (2 << 4) /* TX NAK */ |
			  (1 << 8) /* Double buffered */ |
			  (0 << 9) /* Bulk EP */ |
			  (0 << 12) /* RX Disabled */;

	/* Serial Bulk OUT endpoint 2 */
	desc->ep[2].sb.rx_addr = usb_sram_addr(ep2_rx);
	desc->ep[2].sb.tx_count = 0;
	desc->ep[2].sb.rx_count = 0x8000 | ((MAX_PACKET_SIZE/32-1) << 10);
	STM32_USB_EP(2) = (USB_EP_SERIAL_RX << 0) /*Endpoint Address: 2 */ |
			  (0 << 4) /* TX Disabled */ |
			  (0 << 9) /* Bulk EP */ |
			  (3 << 12) /* RX VALID */;

	/*
	 * set the default address : 0
	 * as we are not configured yet
	 */
	STM32_USB_DADDR = 0 | 0x80;
	CPRINTF("RST EP0 %04x\n", STM32_USB_EP(0));
}

void IRQ_HANDLER(STM32_IRQ_USB_LP)(void)
{
	uint16_t status = STM32_USB_ISTR;

	if ((status & (1 << 10)))
		usb_reset();

	if (status & (1 << 15)) {
		int ep_slot = status & 0x001f;
		if (ep_slot != 1)
			ep_callback[ep_slot]();
	}

	/* ack interrupts */
	STM32_USB_ISTR = 0;
	CPRINTF("USB:%04x/%04x EP0 %04x FNR %04x\n", status, STM32_USB_ISTR,
		STM32_USB_EP(0), STM32_USB_FNR);
}

void IRQ_HANDLER(STM32_IRQ_USB_HP)(void)
{
	STM32_USB_EP(1) &= EP_MASK;
	desc->ep[1].db_tx[!get_dtog()].count = 0;
}

void usb_init(void)
{
	/* Enable USB device clock. */
	STM32_RCC_APB1ENR |= 0x00800000;

	/* PA11/PA12 need to be set in USB alternate mode in the board code */
	/* gpio_config_module(MODULE_USB, 1); */

	/* we need a proper 48MHz clock */
	/*clock_enable_module(MODULE_USB, 1);*/

	/* power on sequence */

	/* keep FRES (USB reset) and remove PDWN (power down) */
	STM32_USB_CNTR = 0x01;
	udelay(1); /* startup time */
	/* reset FRES and keep interrupts masked */
	STM32_USB_CNTR = 0x00;
	/* clear pending interrupts */
	STM32_USB_ISTR = 0;

	/* set descriptors table offset in dedicated SRAM */
	STM32_USB_BTABLE = 0;

	/* EXTI18 is USB wake up interrupt */
	/* STM32_EXTI_RTSR |= 1 << 18; */
	/* STM32_EXTI_IMR |= 1 << 18; */

	/* Enable interrupt handlers */
	task_enable_irq(STM32_IRQ_USB_LP);
	task_enable_irq(STM32_IRQ_USB_HP);

	/* set interrupts mask : reset/correct tranfer/errors */
	STM32_USB_CNTR = 0xe400;

	/* set pull-up on DP for FS mode */
#ifdef CHIP_VARIANT_STM32L15X
	STM32_SYSCFG_PMC |= 1;
#elif defined(CHIP_FAMILY_STM32F0)
	STM32_USB_BCDR |= 1 << 15 /* DPPU */;
#else
	/* hardwired or regular GPIO on other platforms */
#endif

	CPRINTF("USB init done\n");
}
