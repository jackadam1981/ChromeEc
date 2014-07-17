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
#include "hwtimer.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"
#include "usb_ftdi.h"

/*
 * Implements the FTDI MPSSE protocol on an emulated FT232H.
 * Supports only interface A, so USB_IFACE_FTDI must be 0, TX (USB_EP_FTDI_TX)
 * must be 0x81, and RX (USB_EP_FTDI_RX) must be 0x02.
 */

#define USB_FTDI_CMD_CLOCK_WRITE_BYTE_LSB_PCE   0x18
#define USB_FTDI_CMD_CLOCK_WRITE_BYTE_LSB_MCE   0x19
#define USB_FTDI_CMD_CLOCK_WRITE_TMS_BIT_PCE    0x4a
#define USB_FTDI_CMD_CLOCK_WRITE_TMS_BIT_MCE    0x4b
#define USB_FTDI_CMD_WRITE_LOW_BYTE             0x80
#define USB_FTDI_CMD_READ_LOW_BYTE              0x81
#define USB_FTDI_CMD_WRITE_HIGH_BYTE            0x82
#define USB_FTDI_CMD_READ_HIGH_BYTE             0x83
#define USB_FTDI_CMD_LOOPBACK_ON                0x84
#define USB_FTDI_CMD_LOOPBACK_OFF               0x85
#define USB_FTDI_CMD_SET_CLK_DIV                0x86
#define USB_FTDI_CMD_SEND_IMMED                 0x87
#define USB_FTDI_CMD_WAIT_IO_HIGH               0x88
#define USB_FTDI_CMD_WAIT_IO_LOW                0x89
#define USB_FTDI_CMD_CLK_DIV_5_OFF              0x8a
#define USB_FTDI_CMD_CLK_DIV_5_ON               0x8b
#define USB_FTDI_CMD_CLOCK_3_PHASE_ON           0x8c
#define USB_FTDI_CMD_CLOCK_3_PHASE_OFF          0x8d
#define USB_FTDI_CMD_CLOCK_COUNT_BIT            0x8e
#define USB_FTDI_CMD_CLOCK_COUNT_BYTE           0x8f
#define USB_FTDI_CMD_READ_SHORT_ADDR            0x90
#define USB_FTDI_CMD_READ_EXTEND_ADDR           0x91
#define USB_FTDI_CMD_WRITE_SHORT_ADDR           0x92
#define USB_FTDI_CMD_WRITE_EXTEND_ADDR          0x93
#define USB_FTDI_CMD_ADAPT_CLOCK_ON             0x96
#define USB_FTDI_CMD_ADAPT_CLOCK_OFF            0x97
#define USB_FTDI_CMD_CLOCK_UNTIL_HIGH           0x9c
#define USB_FTDI_CMD_CLOCK_UNTIL_LOW            0x9d
#define USB_FTDI_BAD_CMD                        0xfa

/*
 * FT232H JTAG Pin Configuration (JTAGkey-compatible):
 * ADBUS0 (13): TCK/SK      : EC_JTAG_TCK_GATED
 * ADBUS1 (14): TDI/DO      : EC_JTAG_TDI_GATED
 * ADBUS2 (15): TDO/DI      : EC_JTAG_TDO
 * ADBUS3 (16): TMS/CS      : EC_JTAG_TMS
 * ADBUS4 (17): JTAG_OE_EN  : PD_DEBUG_EN_MCU
 * ADBUS5 (18): VREF_IN_N   : HIGH
 * ADBUS6 (19): SRST_IN     : EC_RST_L
 * ACBUS0 (21): TRST_OUT    :
 * ACBUS1 (25): SRST_OUT    : EC_RST_L
 * ACBUS2 (26): TRST_OE_N   : HIGH
 * ACBUS3 (27): SRST_OE_N   : HIGH
 */

#define TIM_JTAG_TCK				3

#define ADBUS_TCK				(1 << 0)
#define ADBUS_TDI				(1 << 1)
#define ADBUS_TDO				(1 << 2)
#define ADBUS_TMS				(1 << 3)
#define ADBUS_JTAG_OE		(1 << 4)
#define ADBUS_VREF			(1 << 5)
#define ADBUS_SRST_IN		(1 << 6)
#define ACBUS_TRST_OUT	(1 << 0)
#define ACBUS_SRST_OUT	(1 << 1)
#define ACBUS_TRST_OE		(1 << 2)
#define ACBUS_SRST_OE		(1 << 3)

/* FTDI descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_FTDI) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = USB_IFACE_FTDI,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_FTDI, USB_EP_FTDI_TX) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x80 | USB_EP_FTDI_TX,
	.bmAttributes = 0x02 /* Bulk IN */,
	.wMaxPacketSize = USB_FTDI_PACKET_SIZE,
	.bInterval = 0,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_FTDI, USB_EP_FTDI_RX) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_EP_FTDI_RX,
	.bmAttributes = 0x02 /* Bulk OUT */,
	.wMaxPacketSize = USB_FTDI_PACKET_SIZE,
	.bInterval = 0,
};

/* Hardware buffers for USB endpoints */
usb_uint ftdi_ep_tx[USB_FTDI_PACKET_SIZE] __usb_ram;
usb_uint ftdi_ep_rx[USB_FTDI_PACKET_SIZE] __usb_ram;

/* Software buffer for FTDI commands that span multiple USB packets */
// static uint8_t cmd[USB_FTDI_BUFFER_SIZE];
// static int cmd_len;
// static int cmd_pos;

/* Bitbang operations */
static uint8_t bitbang_op;
static uint8_t *bitbang_args;
// static int bitbang_pos;
static int bitbang_len;

/* State variables */
static int clk_div5 = 1;

static void ftdi_tx_reset(void)
{
	btable_ep[USB_EP_FTDI_TX].tx_addr = usb_sram_addr(ftdi_ep_tx);
	btable_ep[USB_EP_FTDI_TX].tx_count = 0;
	btable_ep[USB_EP_FTDI_TX].rx_count = 0;

	STM32_USB_EP(USB_EP_FTDI_TX) =
				(USB_EP_FTDI_TX << 0) /* Endpoint Address */ |
				(2 << 4) /* TX NAK */ |
				(0 << 9) /* Bulk EP */ |
				(0 << 12) /* RX Disabled */;
}

static void ftdi_rx_reset(void)
{
	btable_ep[USB_EP_FTDI_RX].rx_addr = usb_sram_addr(ftdi_ep_rx);
	btable_ep[USB_EP_FTDI_RX].rx_count = 0x8000 |
				((USB_FTDI_PACKET_SIZE/32-1) << 10);
	btable_ep[USB_EP_FTDI_RX].tx_count = 0;

	STM32_USB_EP(USB_EP_FTDI_RX) =
				(USB_EP_FTDI_RX << 0) /* Endpoint Address */ |
				(0 << 4) /* TX Disabled */ |
				(0 << 9) /* Bulk EP */ |
				(3 << 12) /* RX VALID */;
}

static void ftdi_send_byte(uint8_t *src, int len)
{
ccprintf("send: %x %x %x\n", src[0], src[1], len);
	memcpy_usbram(ftdi_ep_tx, src, len * sizeof(*src));
	btable_ep[USB_EP_FTDI_TX].tx_count = len * sizeof(*src);

	/* wait for data to be read */
	STM32_TOGGLE_EP(USB_EP_FTDI_TX, EP_TX_MASK, EP_TX_VALID, 0);
}

static void ftdi_tx(void)
{
	STM32_USB_EP(USB_EP_FTDI_TX) &= EP_MASK;
}

static void ftdi_rx(void)
{
	task_set_event(TASK_ID_USB_FTDI, TASK_EVENT_WAKE, 0);

	STM32_USB_EP(USB_EP_FTDI_RX) &= EP_MASK;
}
USB_DECLARE_EP(USB_EP_FTDI_TX, ftdi_tx, ftdi_tx, ftdi_tx_reset);
USB_DECLARE_EP(USB_EP_FTDI_RX, ftdi_rx, ftdi_rx, ftdi_rx_reset);

/* TODO put static */
void ftdi_iface_request(usb_uint *ep0_buf_rx, usb_uint *ep0_buf_tx)
{
	uint16_t *req = (uint16_t *) ep0_buf_rx;

	if ((req[0] & (USB_DIR_IN | USB_RECIP_MASK | USB_TYPE_MASK)) ==
	    (USB_DIR_IN | USB_RECIP_DEVICE | USB_TYPE_VENDOR)) {
		switch (req[0] >> 8) {
		case USB_FTDI_REQ_GET_LAT_TIMER:
			/* return default value */
			ep0_buf_tx[0] = 0x02;
			btable_ep[0].tx_count = sizeof(uint8_t);
			STM32_TOGGLE_EP(USB_EP_CONTROL, EP_TX_RX_MASK,
				EP_TX_RX_VALID, EP_STATUS_OUT /*null OUT transaction */);
			break;
		}
	} else if ((req[0] & (USB_DIR_OUT | USB_RECIP_MASK | USB_TYPE_MASK)) ==
	    (USB_DIR_OUT | USB_RECIP_DEVICE | USB_TYPE_VENDOR)) {
		switch (req[0] >> 8) {
		case USB_FTDI_REQ_SET_BIT_MODE:
			/* only MPSSE mode supported */
			if (!(req[1] & USB_FTDI_BITMODE_MPSSE))
				STM32_TOGGLE_EP(USB_EP_CONTROL, EP_TX_RX_MASK,
				EP_RX_STALL | EP_TX_VALID, 0);
			/* fall through to default handler */
		default:
			/* silently acknowledge */
			btable_ep[0].tx_count = 0;
			STM32_TOGGLE_EP(USB_EP_CONTROL, EP_TX_RX_MASK,
				EP_TX_RX_VALID, 0);
			break;
		}
	} else {
		ccprintf("ftdi stalling: %x %x %x %x\n",
		  req[0], req[1], req[2], req[3]);
		STM32_TOGGLE_EP(USB_EP_CONTROL, EP_TX_RX_MASK,
			EP_RX_VALID | EP_TX_STALL, 0);
	}
}

void ftdi_set_byte(int high, uint8_t value, uint8_t direction)
{
//ccprintf("set: %02x %02x %02x\n", high, value, direction);

	if (!high) {
		if (direction & ADBUS_TCK)
			gpio_set_flags(GPIO_EC_JTAG_TCK,
				(value & ADBUS_TCK) ? GPIO_OUT_HIGH : GPIO_OUT_LOW);
		else
			gpio_set_flags(GPIO_EC_JTAG_TCK, GPIO_INPUT);

		if (direction & ADBUS_TDI)
			gpio_set_flags(GPIO_EC_JTAG_TDI,
				(value & ADBUS_TDI) ? GPIO_OUT_HIGH : GPIO_OUT_LOW);
		else
			gpio_set_flags(GPIO_EC_JTAG_TDI, GPIO_INPUT);

		if (direction & ADBUS_TDO)
			gpio_set_flags(GPIO_EC_JTAG_TDO,
				(value & ADBUS_TDO) ? GPIO_OUT_HIGH : GPIO_OUT_LOW);
		else
			gpio_set_flags(GPIO_EC_JTAG_TDO, GPIO_INPUT);

		if (direction & ADBUS_TMS)
			gpio_set_flags(GPIO_EC_JTAG_TMS,
				(value & ADBUS_TMS) ? GPIO_OUT_HIGH : GPIO_OUT_LOW);
		else
			gpio_set_flags(GPIO_EC_JTAG_TMS, GPIO_INPUT);

		if (!(direction & ADBUS_JTAG_OE))
			/* input only pin */
			gpio_set_flags(GPIO_PD_DEBUG_EN, GPIO_INPUT);
	}	else {
		if (direction & ACBUS_SRST_OUT)
			gpio_set_flags(GPIO_EC_RST_L, /* inverted */
				(value & ACBUS_SRST_OUT) ? GPIO_OUT_LOW : GPIO_OUT_HIGH);
		else
			gpio_set_flags(GPIO_EC_RST_L, GPIO_INPUT);
	}
}

uint8_t ftdi_read_byte(int high)
{
	uint8_t ret = 0;
ccprintf("read: %x\n", high);
	if (!high) {
		ret = (gpio_get_level(GPIO_EC_JTAG_TCK) << 0)
				| (gpio_get_level(GPIO_EC_JTAG_TDI) << 1)
				| (gpio_get_level(GPIO_EC_JTAG_TDO) << 2)
				| (gpio_get_level(GPIO_EC_JTAG_TMS) << 3)
				| (gpio_get_level(GPIO_PD_DEBUG_EN) << 4)
				| (1 << 5) /* always powered */
				| (~(gpio_get_level(GPIO_EC_RST_L) & 0x1) << 6);
	}	else {
		ret = (~(gpio_get_level(GPIO_EC_RST_L) & 0x1) << 1)
				| (1 << 2) /* always output enable */
				| (1 << 3); /* always output enable */
	}

	return ret;
}

void ftdi_set_clock(uint8_t low, uint8_t high)
{
	timer_ctlr_t *tim = (void *) STM32_TIM_BASE(TIM_JTAG_TCK);

	int clk = (clk_div5 ? 12000000 : 60000000) /
		(2 * (1 + ((high << 8) | low)));
ccprintf("clock: %d\n", clk);

	if (!tim->cr1 & (1 << 0)) {
		/* Set alternate function */
		STM32_GPIO_MODER(GPIO_B) &= ~0x3000;
		STM32_GPIO_MODER(GPIO_B) |= 0x2000;

		/* Set AF0 */
		STM32_GPIO_AFRL(GPIO_B) &= ~0xf000000;

		/* Set high speed */
		STM32_GPIO_AFRL(GPIO_B) |= 0x3000;

		/* Reset peripheral */
		STM32_RCC_APB1RSTR |= (1 << 1);
		STM32_RCC_APB1RSTR &= ~(1 << 1);

		/* Enable timer clock */
		__hw_timer_enable_clock(TIM_JTAG_TCK, 1);

		/* Upcounting edge-aligned mode, interrupt only from under/overflow */
		tim->cr1 = (1 << 7) | (1 << 2);

		/* Use TIMx_CH1 to TI1 */
		tim->cr2 = 0;

		/* No master/slave mode */
		tim->smcr = 0;

		/* Update interrupt enable */
		tim->dier = (1 << 0);

		/* Clear interrupt flags */
		tim->sr = 0;

		/* Reload prescaler, reset counter, enable update generation */
		tim->egr = (1 << 0);

		/* Set output compare 1 preload enable and PWM mode 1 */
		tim->ccmr1 = (6 << 4) | (1 << 3);

		/* No output compare 3, 4 */
		tim->ccmr2 = 0;

		/* Set capture/compare channel 1 enable and polarity based on TCK */
		tim->ccer = (gpio_get_level(GPIO_EC_JTAG_TCK) << 1) | (1 << 0);

		/* Clear counter */
		tim->cnt = 0;

		/* Set prescaler to /1 */
		tim->psc = 0;

		/* Set main output enable */
		tim->bdtr = (1 << 15);

		/* Enable IRQ for bit banging */
		task_enable_irq(STM32_IRQ_TIM3);
	}

	/* Set auto-reload value */
	tim->arr = clock_get_freq() / (2 * clk);

	/* Set 50% duty cycle on output */
	tim->ccr[1] = tim->arr / 2;
}

void ftdi_prepare_bitbang(uint8_t op, uint8_t *args, int len)
{
	timer_ctlr_t *tim = (void *) STM32_TIM_BASE(TIM_JTAG_TCK);
ccprintf("bitbang: %02x %02x\n", op, len);
	/* Wait for existing operation to complete */
	while (bitbang_op) {
		ccputs("sleeping...\n");
		usleep(USB_FTDI_SLEEP_USEC);
	}

	/* Prepare for interrupt handler */
	bitbang_op = op;
	bitbang_args = args;
	bitbang_len = len;

	/* Enable the timer */
	tim->cr1 |= (1 << 0);
}

void ftdi_bitbang(void)
{
// 	timer_ctlr_t *tim = (void *) STM32_TIM_BASE(TIM_JTAG_TCK);

// ccprintf("bang: %x\n", bitbang_op);
// 	/* Disable timer */
// 	if (bitbang_pos * 8 >= bitbang_len) {
// 		bitbang_op = 0;
// 		tim->cr1 &= ~(1 << 0);
// 		task_disable_irq(STM32_IRQ_TIM3);
// 	}

// 	switch (bitbang_op) {
// 		case USB_FTDI_CMD_CLOCK_WRITE_TMS_BIT_PCE:
// 		case USB_FTDI_CMD_CLOCK_WRITE_TMS_BIT_MCE:
// 			gpio_set_level(GPIO_EC_JTAG_TMS,
// 					bitbang_args[bitbang_pos / 8] & (1 << (bitbang_pos % 8)));
// 		break;

// 		case USB_FTDI_CMD_CLOCK_WRITE_BYTE_LSB_PCE:
// 		case USB_FTDI_CMD_CLOCK_WRITE_BYTE_LSB_MCE:
// 			gpio_set_level(GPIO_EC_JTAG_TDO,
// 					bitbang_args[bitbang_pos / 8] & (1 << (bitbang_pos % 8)));
// 		break;

// 		case USB_FTDI_CMD_CLOCK_UNTIL_HIGH:
// 		case USB_FTDI_CMD_CLOCK_UNTIL_LOW:
// 			/* do nothing */
// 		break;

// 		default:
// 		break;
// 	}

// 	/* Clear update interrupt flag */
// 	tim->sr = 0;

 	task_clear_pending_irq(STM32_IRQ_TIM3);
}
#ifndef BOARD_SAMUS_PD
DECLARE_IRQ(STM32_IRQ_TIM3, ftdi_bitbang, 1);
#endif

void ftdi_task(void)
{
	uint8_t *msg;
	uint8_t buffer[2] = {0};
	int pos, len;

	while (1) {
		/* wait for event or usb reset */
ccputs("waiting...\n");
		task_wait_event(-1);
ccputs("resuming...\n");

/*
		// check for unfinished command from last packet
		if (!cmd_len) {
			// no unfinished command
*/
			pos = 0;
			msg = (uint8_t *) ftdi_ep_rx;
			len = btable_ep[USB_EP_FTDI_RX].rx_count & 0x3ff;
/*
		} else {
ccprintf("unfinished: %x %x/%x\n", cmd[0], cmd_pos, cmd_len);
			// command cannot fit in buffer
			if (cmd_len > USB_FTDI_BUFFER_SIZE) {
				ccputs("cmd too big!!!\n");
				return;
			}	else if (cmd_len - cmd_pos > (btable_ep[USB_EP_FTDI_RX].rx_count & 0x3ff)) {
				// copy more data, but command still unfinished
				memcpy(cmd + cmd_pos, ftdi_ep_rx, btable_ep[USB_EP_FTDI_RX].rx_count & 0x3ff);
				cmd_pos += btable_ep[USB_EP_FTDI_RX].rx_count & 0x3ff;
				goto end;
			} else {
				// finish command and execute
				memcpy(cmd + cmd_pos, ftdi_ep_rx, cmd_len - cmd_pos);
				pos = 0;
				msg = cmd;
				len = cmd_len;
			}
		}
*/

//parse:
		while (pos < len) {
ccprintf("cmd: %02x %02x/%02x\n", msg[pos], pos, len);
			switch (msg[pos]) {
				case USB_FTDI_CMD_CLOCK_WRITE_TMS_BIT_PCE:
				case USB_FTDI_CMD_CLOCK_WRITE_TMS_BIT_MCE:
					if (len - pos > 2) {
						gpio_set_level(GPIO_EC_JTAG_TDI, msg[pos + 2] & (1 << 7));
						ftdi_prepare_bitbang(msg[pos], &(msg[pos + 2]), 1);
						pos += 3;
					}
					/* else {
ccputs("clock write unfinished\n");
						cmd_pos = len - pos;
						cmd_len = msg[pos + 1] + 2;
						memcpy(cmd, &msg[pos], len - pos);
						goto end;
					} */
				break;

				case USB_FTDI_CMD_WRITE_LOW_BYTE:
				case USB_FTDI_CMD_WRITE_HIGH_BYTE:
					if (len - pos > 2) {
						ftdi_set_byte((msg[pos] & 0x2) ? 1 : 0, msg[pos + 1], msg[pos + 2]);
						pos += 3;
					}
					/* else {
ccputs("cmd write unfinished\n");
						cmd_pos = len - pos;
						cmd_len = 3;
						memcpy(cmd, &msg[pos], len - pos);
						goto end;
					} */
				break;

				case USB_FTDI_CMD_READ_LOW_BYTE:
				case USB_FTDI_CMD_READ_HIGH_BYTE:
					buffer[0] = ftdi_read_byte((msg[pos] & 0x2) ? 1 : 0);
					ftdi_send_byte(buffer, 1);
					pos += 1;
				break;


				case USB_FTDI_CMD_LOOPBACK_ON:
				break;
				case USB_FTDI_CMD_LOOPBACK_OFF:
					pos += 1;
				break;

				case USB_FTDI_CMD_SET_CLK_DIV:
					if (len - pos > 2) {
						ftdi_set_clock(msg[pos + 1], msg[pos + 2]);
						pos += 3;
					}
					/* else {
ccputs("set clk unfinished\n");
						cmd_pos = len - pos;
						cmd_len = 3;
						memcpy(cmd, &msg[pos], len - pos);
						goto end;
					} */
				break;

				case USB_FTDI_CMD_CLOCK_WRITE_BYTE_LSB_PCE:
				case USB_FTDI_CMD_CLOCK_WRITE_BYTE_LSB_MCE:
					if (len - pos > 2 && len - pos > 2 + 1 + ((msg[pos + 2] << 8) | msg[pos + 1])) {
						ftdi_prepare_bitbang(0, NULL, 0);
						// ftdi_prepare_bitbang(msg[pos], &(msg[pos + 3]),
						//   1 + ((msg[pos + 2] << 8) | msg[pos + 1]));
						pos += 3 + 1 + ((msg[pos + 2] << 8) | msg[pos + 1]);
					}
				// 	/* else {
				// 		ccputs("clock write byte unfinished\n");
				// 		cmd_pos = len - pos;
				// 		cmd_len = ((msg[pos + 2] << 8) | msg[pos + 1]) + 3;
				// 		memcpy(cmd, &msg[pos], len - pos);
				// 		goto end;
				// 	} */
				break;
/*
				case USB_FTDI_CMD_SEND_IMMED:
				break;
				case USB_FTDI_CMD_WAIT_IO_HIGH:
				break;
				case USB_FTDI_CMD_WAIT_IO_LOW:
				break;
*/
				case USB_FTDI_CMD_CLK_DIV_5_OFF:
				case USB_FTDI_CMD_CLK_DIV_5_ON:
					clk_div5 = msg[pos] & (1 << 0);
					pos += 1;
				break;
/*
				case USB_FTDI_CMD_CLOCK_3_PHASE_ON:
				break;
				case USB_FTDI_CMD_CLOCK_3_PHASE_OFF:
				break;
				case USB_FTDI_CMD_CLOCK_COUNT_BIT:
				break;
				case USB_FTDI_CMD_CLOCK_COUNT_BYTE:
				break;
				case USB_FTDI_CMD_READ_SHORT_ADDR:
				break;
				case USB_FTDI_CMD_READ_EXTEND_ADDR:
				break;
				case USB_FTDI_CMD_WRITE_SHORT_ADDR:
				break;
				case USB_FTDI_CMD_WRITE_EXTEND_ADDR:
				break;
*/
				case USB_FTDI_CMD_ADAPT_CLOCK_ON:
				break;
				case USB_FTDI_CMD_ADAPT_CLOCK_OFF:
					pos += 1;
				break;
/*
				case USB_FTDI_CMD_CLOCK_UNTIL_HIGH:
				case USB_FTDI_CMD_CLOCK_UNTIL_LOW:
					if (len - pos > 2) {
						gpio_set_level(GPIO_EC_JTAG_TDO, 0);
						ftdi_prepare_bitbang(msg[pos], NULL,
						  1 + ((msg[pos + 2] << 8) | msg[pos + 1]));
						pos += 3;
					}
				break;
*/
				default:
ccprintf("bad: %x\n", msg[pos]);
					buffer[0] = USB_FTDI_BAD_CMD;
					buffer[1] = msg[pos];
					//ftdi_send_byte(buffer, 2);
					pos += 1;

					pos = len;
				break;
			}
		}
ccputs("done\n");
/*
		// finished executing previous command, return to USB buffer
		if (cmd_len) {
			pos = cmd_len - cmd_pos;
			msg = (uint8_t *) ftdi_ep_rx;
			len = btable_ep[USB_EP_FTDI_RX].rx_count & 0x3ff;

			cmd_pos = 0;
			cmd_len = 0;

			goto parse;
		}
*/
		/* wait for bitbang to complete */
		// if (!bitbang_op)
		// 	STM32_TOGGLE_EP(USB_EP_FTDI_RX, EP_RX_MASK, EP_RX_VALID, 0);
	}
}
