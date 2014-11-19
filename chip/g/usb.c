/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#ifdef CONFIG_USB_BOS
/* v2.01 (vs 2.00) BOS Descriptor provided */
#define USB_DEV_BCDUSB 0x0201
#else
#define USB_DEV_BCDUSB 0x0200
#endif

#ifndef USB_DEV_CLASS
#define USB_DEV_CLASS USB_CLASS_PER_INTERFACE
#endif

#ifndef CONFIG_USB_BCD_DEV
#define CONFIG_USB_BCD_DEV 0x0100 /* 1.00 */
#endif

/* USB Standard Device Descriptor */
static const struct usb_device_descriptor dev_desc = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = USB_DEV_BCDUSB,
	.bDeviceClass = USB_DEV_CLASS,
	.bDeviceSubClass = 0x00,
	.bDeviceProtocol = 0x00,
	.bMaxPacketSize0 = USB_MAX_PACKET_SIZE,
	.idVendor = USB_VID_GOOGLE,
	.idProduct = CONFIG_USB_PID,
	.bcdDevice = CONFIG_USB_BCD_DEV,
	.iManufacturer = USB_STR_VENDOR,
	.iProduct = USB_STR_PRODUCT,
	.iSerialNumber = 0,
	.bNumConfigurations = 1
};

/* USB Configuration Descriptor */
const struct usb_config_descriptor USB_CONF_DESC(conf) = {
	.bLength = USB_DT_CONFIG_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0x0BAD, /* no of returned bytes, set at runtime */
	.bNumInterfaces = USB_IFACE_COUNT,
	.bConfigurationValue = 1,
	.iConfiguration = USB_STR_VERSION,
	.bmAttributes = 0x80, /* bus powered */
	.bMaxPower = 250, /* MaxPower 500 mA */
};

const uint8_t usb_string_desc[] = {
	4, /* Descriptor size */
	USB_DT_STRING,
	0x09, 0x04 /* LangID = 0x0409: U.S. English */
};

/* Endpoint table in USB controller RAM */
//struct stm32_endpoint btable_ep[USB_EP_COUNT]
//	__attribute__((section(".usb_ram.btable")));
static struct g_usb_desc ep0_out_desc;
static struct g_usb_desc ep0_in_desc;

/* Control endpoint (EP0) buffers */
static usb_uint ep0_buf_tx[USB_MAX_PACKET_SIZE / 2] /*__usb_ram*/;
static usb_uint ep0_buf_rx[USB_MAX_PACKET_SIZE / 2] /*__usb_ram*/;


static int set_addr;
/* remaining size of descriptor data to transfer */
static int desc_left;
/* pointer to descriptor data if any */
static const uint8_t *desc_ptr;

/* Requests on the control endpoint (aka EP0) */
static void ep0_rx(void)
{
	uint32_t epint = GR_USB_DIEPINT(0);
	uint16_t req = ep0_buf_rx[0]; /* bRequestType | bRequest */

	CPRINTF("rxEPINT %08x\n", epint); /* DEBUG ONLY */

	/* reset any incomplete descriptor transfer */
	desc_ptr = NULL;

	/* interface specific requests */
	if ((req & USB_RECIP_MASK) == USB_RECIP_INTERFACE) {
		uint8_t iface = ep0_buf_rx[2] & 0xff;
		if (iface < USB_IFACE_COUNT)
			usb_iface_request[iface](ep0_buf_rx, ep0_buf_tx);
		return;
	}

	/* TODO check setup bit ? */
	if (req == (USB_DIR_IN | (USB_REQ_GET_DESCRIPTOR << 8))) {
		uint8_t type = ep0_buf_rx[1] >> 8;
		uint8_t idx = ep0_buf_rx[1] & 0xff;
		const uint8_t *desc;
		int len;

		switch (type) {
		case USB_DT_DEVICE: /* Setup : Get device descriptor */
			desc = (void *)&dev_desc;
			len = sizeof(dev_desc);
			break;
		case USB_DT_CONFIGURATION: /* Setup : Get configuration desc */
			desc = __usb_desc;
			len = USB_DESC_SIZE;
			break;
#ifdef CONFIG_USB_BOS
		case USB_DT_BOS: /* Setup : Get BOS descriptor */
			desc = bos_ctx.descp;
			len = bos_ctx.size;
			break;
#endif
		case USB_DT_STRING: /* Setup : Get string descriptor */
			if (idx >= USB_STR_COUNT)
				/* The string does not exist : STALL */
				goto unknown_req;
			desc = usb_strings[idx];
			len = desc[0];
			break;
		case USB_DT_DEVICE_QUALIFIER: /* Get device qualifier desc */
			/* Not high speed : STALL next IN used as handshake */
			goto unknown_req;
		default: /* unhandled descriptor */
			goto unknown_req;
		}
		/* do not send more than what the host asked for */
		len = MIN(ep0_buf_rx[3], len);
		/*
		 * if we cannot transmit everything at once,
		 * keep the remainder for the next IN packet
		 */
		if (len >= USB_MAX_PACKET_SIZE) {
			desc_left = len - USB_MAX_PACKET_SIZE;
			desc_ptr = desc + USB_MAX_PACKET_SIZE;
			len = USB_MAX_PACKET_SIZE;
		}
		memcpy_usbram(ep0_buf_tx, desc, len);
		if (type == USB_DT_CONFIGURATION)
			/* set the real descriptor size */
			ep0_buf_tx[1] = USB_DESC_SIZE;
//		btable_ep[0].tx_count = len;
//		STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
//				desc_left ? 0 : EP_STATUS_OUT);
		/* send the null OUT transaction if the transfer is complete */
	} else if (req == (USB_DIR_IN | (USB_REQ_GET_STATUS << 8))) {
//		uint16_t zero = 0;
		/* Get status */
//		memcpy_usbram(ep0_buf_tx, (void *)&zero, 2);
//		btable_ep[0].tx_count = 2;
//		STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
//			  EP_STATUS_OUT /*null OUT transaction */);
	} else if ((req & 0xff) == USB_DIR_OUT) {
		switch (req >> 8) {
		case USB_REQ_SET_ADDRESS:
			/* set the address after we got IN packet handshake */
			set_addr = ep0_buf_rx[1] & 0xff;
			/* need null IN transaction -> TX Valid */
//			btable_ep[0].tx_count = 0;
//			STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
			break;
		case USB_REQ_SET_CONFIGURATION:
			/* uint8_t cfg = ep0_buf_rx[1] & 0xff; */
			/* null IN for handshake */
//			btable_ep[0].tx_count = 0;
//			STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
			break;
		default: /* unhandled request */
			goto unknown_req;
		}

	} else {
		goto unknown_req;
	}

	return;
unknown_req:
//	STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_RX_VALID | EP_TX_STALL, 0);
	return;
}

static void ep0_tx(void)
{
	uint32_t epint = GR_USB_DOEPINT(0);
	CPRINTF("txEPINT %08x\n", epint); /* DEBUG ONLY */

	if (set_addr) {
		GR_USB_DCFG = (GR_USB_DCFG & ~DCFG_DEVADDR(0x7f))
			    | DCFG_DEVADDR(set_addr);
		CPRINTF("SETAD %02x\n", set_addr);
		set_addr = 0;
	}
	if (desc_ptr) {
		/* we have an on-going descriptor transfer */
		int len = MIN(desc_left, USB_MAX_PACKET_SIZE);
		memcpy_usbram(ep0_buf_tx, desc_ptr, len);
//		btable_ep[0].tx_count = len;
		desc_left -= len;
		desc_ptr += len;
//		STM32_TOGGLE_EP(0, EP_TX_MASK, EP_TX_VALID,
//				desc_left ? 0 : EP_STATUS_OUT);
		/* send the null OUT transaction if the transfer is complete */
		return;
	}
//	STM32_TOGGLE_EP(0, EP_TX_MASK, EP_TX_VALID, 0);
}

static void ep0_reset(void)
{
#if 0
	STM32_USB_EP(0) = (1 << 9) /* control EP */ |
			  (2 << 4) /* TX NAK */ |
			  (3 << 12) /* RX VALID */;

	btable_ep[0].tx_addr = usb_sram_addr(ep0_buf_tx);
	btable_ep[0].rx_addr = usb_sram_addr(ep0_buf_rx);
	btable_ep[0].rx_count = 0x8000 | ((USB_MAX_PACKET_SIZE/32-1) << 10);
	btable_ep[0].tx_count = 0;
#endif
}
USB_DECLARE_EP(0, ep0_tx, ep0_rx, ep0_reset);

static void usb_reset(void)
{
	int ep;

	for (ep = 0; ep < USB_EP_COUNT; ep++)
		usb_ep_reset[ep]();

	/*
	 * set the default address : 0
	 * as we are not configured yet
	 */
	GR_USB_DCFG &= ~DCFG_DEVADDR(0x7f);
}

void usb_interrupt(void)
{
	uint32_t status = GR_USB_GINTSTS;

	if (status & GINTSTS_ENUMDONE)
		CPRINTF("DSTS %08x\n", GR_USB_DSTS); /* DEBUG FIXME */

	if (status & GINTSTS_USBRST)
		usb_reset();

	if (status & (GINTSTS_OEPINT | GINTSTS_IEPINT)) {
		uint32_t daint = GR_USB_DAINT;
		int ep;
		for (ep = 0; ep < USB_EP_COUNT && daint ; ep++, daint>>=1) {
			if (daint & 1)
				usb_ep_rx[ep]();
			if (daint & (1 << 16))
				usb_ep_tx[ep]();
		}
	}

	if (status & GINTSTS_GOUTNAKEFF)
		GR_USB_DCTL = DCTL_CGOUTNAK;

	if (status & GINTSTS_GINNAKEFF)
		GR_USB_DCTL = DCTL_CGNPINNAK;


	CPRINTF("INT %08x\n", status); /* FIXME */

	/* ack interrupts */
	GR_USB_GINTSTS = 0xFFFFFFFF;
}
DECLARE_IRQ(GC_IRQNUM_USB0_USBINTR, usb_interrupt, 1);

static void usb_softreset(void)
{
	int timeout;

	GR_USB_GRSTCTL = GRSTCTL_CSFTRST;
	timeout = 10000;
	while ((GR_USB_GRSTCTL & GRSTCTL_CSFTRST) && timeout-- > 0)
		;
	if (GR_USB_GRSTCTL & GRSTCTL_CSFTRST) {
		CPRINTF("USB: reset failed\n");
		return;
	}

	timeout = 10000;
	while (!(GR_USB_GRSTCTL & GRSTCTL_AHBIDLE) && timeout-- > 0)
		;
	if (!timeout) {
		CPRINTF("USB: reset timeout\n");
		return;
	}
}

void usb_connect(void)
{
	GR_USB_DCTL &= ~DCTL_SFTDISCON;
}

void usb_disconnect(void)
{
	GR_USB_DCTL |= DCTL_SFTDISCON;
}

void usb_init(void)
{
	int i;
	/* TODO PINMUX GC_PINMUX_USB0_EXT xxx */
	/* TODO clocks */

	usb_softreset();

	ccprintf("EPs %d HWCFG %08x/%08x/%08x/%08x ID %08x\n",
		 (GR_USB_GHWCFG2>>10)&0xF,
		 GR_USB_GHWCFG1, GR_USB_GHWCFG2, GR_USB_GHWCFG3, GR_USB_GHWCFG4,
		 GR_USB_GSNPSID);


	/* unmask subset of endpoint interrupts */
	GR_USB_DIEPMSK = DIEPMSK_TIMEOUTMSK | DIEPMSK_AHBERRMSK |
			 DIEPMSK_EPDISBLDMSK | DIEPMSK_XFERCOMPLMSK |
			 DIEPMSK_INTKNTXFEMPMSK | DIEPMSK_INTKNEPMISMSK;
	GR_USB_DOEPMSK = DOEPMSK_SETUPMSK | DOEPMSK_AHBERRMSK |
			 DOEPMSK_EPDISBLDMSK | DOEPMSK_XFERCOMPLMSK;
	GR_USB_DAINTMSK = 0;

	/* Be in disconnected state we are ready */
	GR_USB_DCTL |= DCTL_SFTDISCON;

	/* Max speed: USB2 FS */
	GR_USB_DCFG = DCFG_DEVSPD_FS | DCFG_DESCDMA;

	/* clear pending interrupts */
	GR_USB_GINTSTS = 0xFFFFFFFF;

	/* Setup FIFOs configuration : total size 1024 bytes */
	CPRINTF("GRXFSIZ=0x%08x, GNPTXFSIZ=0x%08x\n",
		GR_USB_GRXFSIZ, GR_USB_GNPTXFSIZ);
	/* set all FIFO to 64 bytes = 16 x 32-bit words */
	GR_USB_GRXFSIZ = 16; /* USB2.0 FS: 64-byte packets max */
	GR_USB_GNPTXFSIZ = 16 | (16 << 16);
	for (i = 1; i < 16; i++)
		GR_USB_DIEPTXF(i) = (16 + i*16) | (16 << 16);
	/* Flush all FIFOs */
	GR_USB_GRSTCTL = GRSTCTL_TXFNUM(0x10) | GRSTCTL_TXFFLSH | GRSTCTL_RXFFLSH;
	/* TODO wait end of flush : GRSTCTL & GRSTCTL_TXFFLSH | GRSTCTL_RXFFLSH / timeout 100ms */

	/* PHY configuration */
	/* external USB3320 ULPI PHY (8 bit) */
	GR_USB_GUSBCFG = GUSBCFG_ULPI | GUSBCFG_PHYIF8 | GUSBCFG_TOUTCAL(7) | (9 << 10);
	/* Global + DMA configuration */
	GR_USB_GAHBCFG = GAHBCFG_DMA_EN | GAHBCFG_GLB_INTR_EN |
			GAHBCFG_NP_TXF_EMP_LVL;

	/* Initialize endpoints */
	for (i = 0; i < 16; i++) {
		GR_USB_DIEPCTL(i) = 0x00/* TODO  */;
		GR_USB_DOEPCTL(i) = 0x00/* TODO */;
	}
	GR_USB_DAINTMSK = 1 | (1 << 16); /* EPO interrupts */

	GR_USB_DCTL |= DCTL_PWRONPRGDONE;
	udelay(10);
	GR_USB_DCTL &= ~DCTL_PWRONPRGDONE;

	/* Configure endpoint 0 */
	ep0_out_desc.flags = DOEPDMA_RXBYTES(64) | DOEPDMA_LAST | DOEPDMA_BS_HOST_RDY |
			     DOEPDMA_IOC;
	ep0_out_desc.addr = ep0_buf_rx;
	ep0_in_desc.flags = DOEPDMA_LAST | DOEPDMA_BS_HOST_BSY | DOEPDMA_IOC;
	ep0_in_desc.addr = ep0_buf_tx;
	/* not in S/G : GR_USB_DOEPTSIZ(0) = DXEPTSIZ_SUPCNT(1) | DXEPTSIZ_PKTCNT(1) | DXEPTSIZ_XFERSIZE(8);*/
	GR_USB_DOEPCTL(0) = DXEPCTL_MPS64 | DXEPCTL_CNAK | DXEPCTL_EPENA | DXEPCTL_USBACTEP;
	GR_USB_DIEPCTL(0) = DXEPCTL_MPS64 | DXEPCTL_USBACTEP;

	/* Clear global NAKs */
	GR_USB_DCTL = DCTL_CGOUTNAK | DCTL_CGNPINNAK;

	/* Enable interrupt handlers */
	task_enable_irq(GC_IRQNUM_USB0_USBINTR);
	/* set interrupts mask : reset/correct tranfer/errors */
	GR_USB_GINTMSK = GINTSTS_GOUTNAKEFF | GINTSTS_GINNAKEFF |
			 GINTSTS_USBRST | GINTSTS_ENUMDONE |
			 GINTSTS_OEPINT | GINTSTS_IEPINT;

#ifndef CONFIG_USB_INHIBIT_CONNECT
	usb_connect();
#endif

	CPRINTF("USB init done\n");
}
#ifndef CONFIG_USB_INHIBIT_INIT
DECLARE_HOOK(HOOK_INIT, usb_init, HOOK_PRIO_DEFAULT);
#endif

void usb_release(void)
{
	/* signal disconnect to host */
	usb_disconnect();

	/* disable interrupt handlers */
	task_disable_irq(GC_IRQNUM_USB0_USBINTR);

	/* TODO: pin-mux and clock */
}
