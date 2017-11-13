/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hwtimer.h"
#include "hooks.h"
#include "i2c.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "touchpad.h"
#include "update_fw.h"
#include "util.h"
#include "usb_hid_touchpad.h"
/* TODO(stimim): make this an option (respect CONFIG_USB_ISOCHRONOUS) */
#include "usb_isochronous.h"

/* Console output macros */
#define CC_TOUCHPAD CC_USB
#define CPUTS(outstr) cputs(CC_TOUCHPAD, outstr)
#define CPRINTF(format, args...) cprintf(CC_TOUCHPAD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_TOUCHPAD, format, ## args)

#define SPI (&(spi_devices[SPI_ST_TP_DEVICE_ID]))
#define HEAT_MAP_ADDR 0x5020


#define ST_TOUCH_ROWS		24
#define ST_TOUCH_COLS		14

#define ST_TOUCH_HEADER_SIZE	32
/* TODO(stimim): this will become one byte per pixel in the future */
#define ST_TOUCH_FRAME_SIZE	(ST_TOUCH_ROWS * ST_TOUCH_COLS * 2)
#define ST_TOUCH_FORCE_SIZE	(ST_TOUCH_ROWS * 2)
#define ST_TOUCH_SENSE_SIZE	(ST_TOUCH_COLS * 2)

#define ST_TOUCH_MAX_MS_PER_FRAME	30

#define ST_TOUCH_N_FRAME_TYPES	3

struct __attribute__((packed)) packet_header_t {
	uint8_t index;
	uint8_t new_frame:1;
};

static struct packet_header_t packet_header = {
	.index = 0,
	.new_frame = 0,
};

struct __attribute__((packed)) st_tp_frame {
	uint8_t dummy;
	uint8_t frame[ST_TOUCH_FRAME_SIZE];
	uint8_t force[ST_TOUCH_FORCE_SIZE];
	uint8_t sense[ST_TOUCH_SENSE_SIZE];
};

struct st_tp_frame frame_buffer[2]; /* double buffering */
/* next buffer index SPI will write to. */
static volatile uint32_t spi_buffer_index = 0;
/* next buffer index USB will read from */
static volatile uint32_t usb_buffer_index = 0;
uint8_t spi_buffer[32];


static int st_tp_read_frame(void);
static int st_tp_send_ack(void);

static int st_tp_read_report(void)
{
	/* because we are using double buffering, so, if usb_buffer_index = N
	 * 1. spi_buffer_index == N      => ok, both slot is empty
	 * 2. spi_buffer_index == N + 1  => ok, second slot is empty
	 * 3. spi_buffer_index == N + 2  => not ok, need to wait for USB.
	 *
	 * TODO(stimim): can we override second slot for case (3)?
	 */
	if ((uint32_t) (spi_buffer_index - usb_buffer_index) <= 1) {
		st_tp_read_frame();
		atomic_add(&spi_buffer_index, 1);
	}
	st_tp_send_ack();
	return 0;
}

/* Send a command to device and read response back */
static int st_tp_command_response(uint8_t cmd, uint16_t addr,
				  void *rx_buf, int len)
{
	uint8_t tx_buf[] = { cmd, addr >> 8, addr & 0xFF, };
	return spi_transaction(SPI, tx_buf, sizeof(tx_buf), rx_buf, len);
}

static int st_tp_read_frame(void)
{
	return st_tp_command_response(
			0xD0, HEAT_MAP_ADDR,
			&frame_buffer[spi_buffer_index & 1],
			sizeof(struct st_tp_frame));
}

static int st_tp_send_ack(void)
{
	uint8_t tx_buf[2] = {0xAC, 0x00};
	return spi_transaction(SPI, tx_buf, 2, NULL, 0);
}

static int st_tp_start_scan(void)
{
	uint8_t tx_buf[2] = {0x93, 0x00};
	return spi_transaction(SPI, tx_buf, 2, NULL, 0);
}

static int st_tp_stop_scan(void)
{
	uint8_t tx_buf[2] = {0x92, 0x00};
	return spi_transaction(SPI, tx_buf, 2, NULL, 0);
}

static int st_tp_read_chip_id(void)
{
	int ret, i;
	uint8_t hwid[8];
	for (i = 0; i < 7; ++ i) {
		hwid[i] = 0;
	}
	ret = st_tp_command_response(0xB4, 0x0007, hwid, 7);
	CPRINTF("%s: ret = %d\n", __func__, ret);
	for (i = 0; i < 7; ++ i) {
		CPRINTF("%02x\n", hwid[i]);
	}
	return ret;
}

int st_tp_read_one_event(void)
{
	uint8_t cmd_read_one_event = 0x85;
	int ret = spi_transaction(SPI, &cmd_read_one_event, 1,
				  spi_buffer, 9);
	if (ret != EC_SUCCESS) {
		ccprintf("tsc_read_one_event: error = %d\n", ret);
		return ret;
	}
	return 0;
}

void st_tp_reset_by_pin(void)
{
	gpio_set_level(GPIO_EN_PP3300_TP, 0);
	udelay(10 * MSEC);
	gpio_set_level(GPIO_EN_PP3300_TP, 1);
	udelay(100 * MSEC);
	gpio_set_level(GPIO_EN_PP3300_TP, 0);
	udelay(300 * MSEC);
}

static int st_tp_reset(void)
{
#if 0
	return 0;
#else
	int i, ret;
	st_tp_reset_by_pin();
	for (i = 32; i < 32; i++) {
		ret = st_tp_read_one_event();
		if (ret == 0 && spi_buffer[1] == 0x10)
			break;
		msleep(10);
	}
	return ret;
#endif
}

/* Initialize the controller ICs after reset */
static void st_tp_init(void)
{
	int rv = 0;

	rv = st_tp_reset();
	CPRINTS("%s:reset = %d", __func__, rv);
	rv = st_tp_read_chip_id();
	CPRINTS("%s:read_config = %d", __func__, rv);
#if 0
	st_tp_start_scan();
	st_tp_send_ack();
	CPRINTS("%s:enable interrupt", __func__);
	gpio_enable_interrupt(GPIO_TOUCHPAD_INT);
#endif
}
DECLARE_DEFERRED(st_tp_init);

#ifdef CONFIG_USB_UPDATE
int touchpad_get_info(struct touchpad_info *tp)
{
	tp->status = EC_RES_SUCCESS;
	tp->vendor = 0;

	return sizeof(*tp);
}

int touchpad_update_write(int offset, int size, const uint8_t *data)
{
	CPRINTS("%s %08x %d", __func__, offset, size);

	return EC_ERROR_UNIMPLEMENTED;
}

/* TODO(b:XXXX): Implement debugging mode for ST touchpad. */
int touchpad_debug(const uint8_t *param, unsigned int param_size,
		   uint8_t **data, unsigned int *data_size)
{
	return EC_RES_INVALID_COMMAND;
}
#endif

void touchpad_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_TOUCHPAD);
}

void touchpad_task(void *u)
{
	st_tp_init();

	while (1) {
		task_wait_event(-1);

		st_tp_read_report();
	}
}

static size_t transmit_report_offset = 0;

/* USB interface has completed TX, it's asking for more data */
static size_t st_tp_usb_tx_callback(usb_uint *usb_addr, size_t tx_size)
{
	size_t num_byte_available;
	size_t count = 0;
	uintptr_t ptr = usb_sram_addr(usb_addr);
	size_t buffer_index;

	if (usb_buffer_index == spi_buffer_index)
		/* buffer is empty */
		return 0;

	buffer_index = usb_buffer_index % 2;

	num_byte_available = ST_TOUCH_FRAME_SIZE - transmit_report_offset;
	if (num_byte_available > 0) {
		packet_header.new_frame = transmit_report_offset == 0;
		memcpy_to_usbram((void *)ptr,
				 &packet_header,
				 sizeof(packet_header));
		packet_header.index++;
		count += sizeof(packet_header);
		num_byte_available = MIN(tx_size - count, num_byte_available);
		memcpy_to_usbram((void *)(ptr + count),
				 (frame_buffer[buffer_index].frame +
				  transmit_report_offset),
				 num_byte_available);
		transmit_report_offset += num_byte_available;
		count += num_byte_available;

		if (transmit_report_offset == ST_TOUCH_FRAME_SIZE) {
			transmit_report_offset = 0;
			atomic_add(&usb_buffer_index, 1);
		}
	}
	return count;
}

static void st_tp_usb_enable(void)
{
	CPRINTS("%s:start scan", __func__);
	st_tp_start_scan();
	CPRINTS("%s:send ack", __func__);
	st_tp_send_ack();
	CPRINTS("%s:enable interrupt", __func__);
	gpio_enable_interrupt(GPIO_TOUCHPAD_INT);
	CPRINTS("%s:size = %d", __func__, ST_TOUCH_FRAME_SIZE);
}
DECLARE_DEFERRED(st_tp_usb_enable);

static void st_tp_usb_disable(void)
{
	st_tp_stop_scan();
	CPRINTS("%s:disable interrupt", __func__);
	gpio_disable_interrupt(GPIO_TOUCHPAD_INT);
}
DECLARE_DEFERRED(st_tp_usb_disable);

static int st_tp_usb_set_interface(usb_uint alternate_setting,
				   usb_uint interface)
{
	if (alternate_setting == 1) {
		hook_call_deferred(&st_tp_usb_enable_data, 0);
		return 0;
	} else if (alternate_setting == 0) {
		hook_call_deferred(&st_tp_usb_disable_data, 0);
		return 0;
	} else  /* we only have two settings. */
		return -1;
}

USB_ISOCHRONOUS_CONFIG_FULL(usb_st_tp_passthru_config,
			    USB_IFACE_ST_TOUCHPAD,
			    USB_CLASS_VENDOR_SPEC,
			    0,  /* subclass */
			    0,  /* protocol */
			    0,  /* interface name */
			    USB_EP_ST_TOUCHPAD,
			    128,  /* packet size */
			    st_tp_usb_tx_callback,
			    st_tp_usb_set_interface)
