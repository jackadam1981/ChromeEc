/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "printf.h"
#include "queue.h"
#include "registers.h"
#include "task.h"
#include "usb_descriptor.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

/* Need these for case-closed debug */
static int is_enabled = 1;
static int is_readonly;
static int is_reset;

/* Functions to deal with the USB FIFOs outside of interrupt context */
static void usb_console_rx_handler(void);
static void usb_console_tx_handler(void);

/****************************************************************************/
/* We'll use queues to buffer the bytes between the console and USB FIFOs */

#define FROM_HOST_QUEUE_SIZE 100
#define TO_HOST_QUEUE_SIZE 100

static void from_host_add(struct queue_policy const *queue_policy, size_t count)
{
	console_has_input();
}

static void from_host_remove(struct queue_policy const *queue_policy,
			    size_t count)
{
	hook_call_deferred(usb_console_rx_handler, 0);
}

static struct queue_policy const from_host_policy = {
	.add = from_host_add,
	.remove = from_host_remove,
};

static void to_host_add(struct queue_policy const *queue_policy, size_t count)
{
	hook_call_deferred(usb_console_tx_handler, 0);
}

static void to_host_remove(struct queue_policy const *queue_policy,
			    size_t count)
{
	/* we don't care HEY: really?*/
}

static struct queue_policy const to_host_policy = {
	.add = to_host_add,
	.remove = to_host_remove,
};

static struct queue const from_host_q = QUEUE(FROM_HOST_QUEUE_SIZE, uint8_t,
					     from_host_policy);

static struct queue const to_host_q = QUEUE(TO_HOST_QUEUE_SIZE, uint8_t,
					     to_host_policy);


/****************************************************************************/
/* USB interface stuff */

/* USB-Serial descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_CONSOLE) =
{
	.bLength            = USB_DT_INTERFACE_SIZE,
	.bDescriptorType    = USB_DT_INTERFACE,
	.bInterfaceNumber   = USB_IFACE_CONSOLE,
	.bAlternateSetting  = 0,
	.bNumEndpoints      = 2,
	.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = USB_SUBCLASS_GOOGLE_SERIAL,
	.bInterfaceProtocol = USB_PROTOCOL_GOOGLE_SERIAL,
	.iInterface         = USB_STR_CONSOLE_NAME,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_CONSOLE, 0) =
{
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = 0x80 | USB_EP_CONSOLE,
	.bmAttributes       = 0x02 /* Bulk IN */,
	.wMaxPacketSize     = USB_MAX_PACKET_SIZE,
	.bInterval          = 10
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_CONSOLE, 1) =
{
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USB_EP_CONSOLE,
	.bmAttributes       = 0x02 /* Bulk OUT */,
	.wMaxPacketSize     = USB_MAX_PACKET_SIZE,
	.bInterval          = 0
};

static uint8_t ep_buf_tx[USB_MAX_PACKET_SIZE];
static uint8_t ep_buf_rx[USB_MAX_PACKET_SIZE];
static struct g_usb_desc ep_out_desc;
static struct g_usb_desc ep_in_desc;

/* Let the USB HW IN-to-host FIFO transmit some bytes */
static void usb_enable_tx(int len)
{
	if (!is_enabled || !is_reset)
		return;

	ep_in_desc.flags = DIEPDMA_LAST | DIEPDMA_BS_HOST_RDY | DIEPDMA_IOC |
			   DIEPDMA_TXBYTES(len);
	GR_USB_DIEPCTL(USB_EP_CONSOLE) |= DXEPCTL_CNAK | DXEPCTL_EPENA;
}

/* Let the USB HW OUT-from-host FIFO receive some bytes */
static void usb_enable_rx(int len)
{
	ep_out_desc.flags = DOEPDMA_RXBYTES(len) |
			    DOEPDMA_LAST | DOEPDMA_BS_HOST_RDY | DOEPDMA_IOC;
	GR_USB_DOEPCTL(USB_EP_CONSOLE) |= DXEPCTL_CNAK | DXEPCTL_EPENA;
}

/* True if the HW Rx/OUT FIFO has bytes for us. */
static inline int rx_fifo_is_ready(void)
{
	return (ep_out_desc.flags & DOEPDMA_BS_MASK) == DOEPDMA_BS_DMA_DONE;
}

/*
 * This function tries to shove new bytes from the USB host into the queue for
 * consumption elsewhere. It is invoked either by a HW interrupt (telling us we
 * have new bytes from the USB host), or by whoever is reading bytes out of the
 * other end of the queue (telling us that there's now more room in the queue
 * if we still have bytes to shove in there).
 */
static void usb_console_rx_handler(void)
{
	/*
	 * The HW FIFO buffer (ep_buf_rx) is always filled from [0] by the
	 * hardware. The rx_in_fifo variable counts how many bytes of that
	 * buffer are actually valid, and is calculated from the HW DMA
	 * descriptor table. The descriptor is updated by the hardware, and it
	 * and ep_buf_rx remains valid and unchanged until software tells the
	 * the hardware engine to accept more input.
	 */
	int rx_in_fifo, rx_left;

	/*
	 * The rx_handled variable tracks how many of the bytes in the HW FIFO
	 * we've copied into the incoming queue. The queue may not accept all
	 * of them at once, so we have to keep track of where we are so that
	 * the next time this function is called we can try to shove the rest
	 * of the HW FIFO bytes into the queue.
	 */
	static int rx_handled;

	/* If the HW FIFO isn't ready, then we're waiting for more bytes */
	if (!rx_fifo_is_ready())
		return;

	/*
	 * How many of the HW FIFO bytes have we not yet handled? We need to
	 * know both where we are in the buffer and how many bytes we haven't
	 * yet enqueued. One can be calculated from the other as long as we
	 * know rx_in_fifo, but we need at least one static variable.
	 */
	rx_in_fifo = USB_MAX_PACKET_SIZE
		- (ep_out_desc.flags & DOEPDMA_RXBYTES_MASK);
	rx_left = rx_in_fifo - rx_handled;

	/* If we have some, try to shove them into the from_host_q */
	if (rx_left) {
		size_t added;

		/* In read-only mode, discard all bytes from the host */
		if (is_readonly)
			added = rx_left;
		else
			added = QUEUE_ADD_UNITS(&from_host_q,
						ep_buf_rx + rx_handled,
						rx_left);
		rx_handled += added;
		rx_left -= added;
	}

	/*
	 * When we've handled all the bytes in the queue ("rx_in_fifo ==
	 * rx_handled" and "rx_left == 0" indicate the same thing), we can
	 * reenable the USB HW to go fetch more.
	 */
	if (!rx_left) {
		rx_handled = 0;
		usb_enable_rx(USB_MAX_PACKET_SIZE);
	}
}
DECLARE_DEFERRED(usb_console_rx_handler);

void console_is_ready_for_more_bytes(void)
{
	hook_call_deferred(usb_console_rx_handler, 0);
}

/* Rx/OUT interrupt handler */
static void con_ep_rx(void)
{
	/* Wake up the Rx FIFO handler */
	hook_call_deferred(usb_console_rx_handler, 0);

	/* clear the RX/OUT interrupts */
	GR_USB_DOEPINT(USB_EP_CONSOLE) = 0xffffffff;
}

/* True if the Tx/IN FIFO can take some bytes from us. */
static inline int tx_fifo_is_ready(void)
{
	uint32_t status = ep_in_desc.flags & DIEPDMA_BS_MASK;
	return status == DIEPDMA_BS_DMA_DONE || status == DIEPDMA_BS_HOST_BSY;
}

/* Try to send some bytes to the host */
static void usb_console_tx_handler(void)
{
	size_t count;

	/* If the HW FIFO isn't ready, then we can't do anything right now. */
	if (!tx_fifo_is_ready())
		return;

	/* Pull bytes out of the to_host_q, put them in the TX FIFO */
	count = QUEUE_REMOVE_UNITS(&to_host_q, ep_buf_tx, USB_MAX_PACKET_SIZE);
	if (count)
		usb_enable_tx(count);
}
DECLARE_DEFERRED(usb_console_tx_handler);

void console_is_ready_to_emit_bytes(void)
{
	hook_call_deferred(usb_console_tx_handler, 0);
}

/* Tx/IN interrupt handler */
static void con_ep_tx(void)
{
	/* Wake up the Tx FIFO handler */
	hook_call_deferred(usb_console_tx_handler, 0);

	/* clear the Tx/IN interrupts */
	GR_USB_DIEPINT(USB_EP_CONSOLE) = 0xffffffff;
}

static void ep_reset(void)
{
	ep_out_desc.flags = DOEPDMA_RXBYTES(USB_MAX_PACKET_SIZE) |
			    DOEPDMA_LAST | DOEPDMA_BS_HOST_RDY | DOEPDMA_IOC;
	ep_out_desc.addr = ep_buf_rx;
	GR_USB_DOEPDMA(USB_EP_CONSOLE) = (uint32_t)&ep_out_desc;
	ep_in_desc.flags = DIEPDMA_LAST | DIEPDMA_BS_HOST_BSY | DIEPDMA_IOC;
	ep_in_desc.addr = ep_buf_tx;
	GR_USB_DIEPDMA(USB_EP_CONSOLE) = (uint32_t)&ep_in_desc;
	GR_USB_DOEPCTL(USB_EP_CONSOLE) = DXEPCTL_MPS(64) | DXEPCTL_USBACTEP |
					 DXEPCTL_EPTYPE_BULK |
					 DXEPCTL_CNAK | DXEPCTL_EPENA;
	GR_USB_DIEPCTL(USB_EP_CONSOLE) = DXEPCTL_MPS(64) | DXEPCTL_USBACTEP |
					 DXEPCTL_EPTYPE_BULK |
					 DXEPCTL_TXFNUM(USB_EP_CONSOLE);
	GR_USB_DAINTMSK |= (DAINT_INEP(USB_EP_CONSOLE) |
			    DAINT_OUTEP(USB_EP_CONSOLE));

	is_reset = 1;

	/* Flush any queued data */
	hook_call_deferred(usb_console_tx_handler, 0);
	hook_call_deferred(usb_console_rx_handler, 0);
}
USB_DECLARE_EP(USB_EP_CONSOLE, con_ep_tx, con_ep_rx, ep_reset);

/****************************************************************************/
/* External console API */

int usb_getc(void)
{
	size_t count;
	uint8_t c8;

	if (!is_enabled)
		return -1;

	count = queue_remove_unit(&from_host_q, &c8);

	if (count)
		return c8;

	return -1;
}

static int __tx_char(void *context, int c32)
{
	size_t added;
	uint8_t c8 = c32;
	int *tx_idx = context;

	/* Do newline to CRLF translation */
	if (c8 == '\n' && __tx_char(context, '\r'))
		return 1;

	added = queue_add_unit(&to_host_q, &c8);
	(*tx_idx)++;

	/* Return 0 if added successfully */
	return !added;
}

int usb_putc(int c)
{
	int ret;
	int tx_idx = 0;

	ret = __tx_char(&tx_idx, c);

	return ret;
}

int usb_puts(const char *outstr)
{
	int tx_idx = 0;

	/* Put all characters in the output buffer */
	while (*outstr) {
		if (__tx_char(&tx_idx, *outstr++) != 0)
			break;
	}

	/* Successful if we consumed all output */
	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_vprintf(const char *format, va_list args)
{
	int ret;
	int tx_idx = 0;

	ret = vfnprintf(__tx_char, &tx_idx, format, args);

	return ret;
}

void usb_console_enable(int enabled, int readonly)
{
	is_enabled = enabled;
	is_readonly = readonly;
}
