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

#define ST_TP_CMD_READ_ALL_EVENTS		0x87
#define ST_TP_CMD_WRITE_SCAN_MODE_SELECT	0xA0
#define ST_TP_CMD_WRITE_FEATURE_SELECT		0xA2
#define ST_TP_CMD_WRITE_SYSTEM_COMMAND		0xA4
#define ST_TP_CMD_WRITE_HOST_DATA_MEMORY	0xA6
#define ST_TP_CMD_READ_HOST_DATA_MEMORY		0xA7
#define ST_TP_CMD_WRITE_FW_CONFIG		0xA8
#define ST_TP_CMD_READ_FW_CONFIG		0xA9
#define ST_TP_CMD_SPI_HOST_BUFFER_ACK		0xC0
#define ST_TP_CMD_READ_SPI_HOST_BUFFER		0xC1

#define ST_HOST_BUFFER_DATA_VALID	(1 << 0)
#define ST_HOST_BUFFER_MT_READY		(1 << 3)
#define ST_HOST_BUFFER_SF_READY		(1 << 4)
#define ST_HOST_BUFFER_SS_READY		(1 << 5)

#define ST_TP_HEAT_MAP_ADDR 0x0020

#define ST_TP_SCAN_MODE_ACTIVE		0x00
#define ST_TP_SCAN_MODE_LOW_POWER	0x01
#define ST_TP_SCAN_MODE_TUNING_WIZARD	0x02
#define ST_TP_SCAN_MODE_LOCKED		0x03

#define ST_TOUCH_ROWS		(18)  /* force len */
#define ST_TOUCH_COLS		(25)  /* sense len */

#define ST_TOUCH_HEADER_SIZE	32

#define BYTES_PER_PIXEL		2
/* Number of bits per pixel, this value is decided by experiments. */
#define BITS_PER_PIXEL		(11)

#define ST_TOUCH_FRAME_SIZE	(ST_TOUCH_ROWS * ST_TOUCH_COLS * BYTES_PER_PIXEL)
#define ST_TOUCH_FORCE_SIZE	(ST_TOUCH_ROWS * BYTES_PER_PIXEL)
#define ST_TOUCH_SENSE_SIZE	(ST_TOUCH_COLS * BYTES_PER_PIXEL)

#define ST_TOUCH_MAX_MS_PER_FRAME	30

#define ST_TOUCH_N_FRAME_TYPES	3

struct packet_header_t {
	uint8_t index;

#define HEADER_FLAGS_NEW_FRAME	(1 << 0)
	uint8_t flags;
} __packed;
static struct packet_header_t packet_header = {
	.index = 0,
	.flags = 0,
};

/*
 * What will be read from touchpad.
 */
struct st_tp_frame {
	uint8_t dummy;
	uint8_t frame[ST_TOUCH_FRAME_SIZE];
#if 0
	/* we are not using these now */
	uint8_t force[ST_TOUCH_FORCE_SIZE];
	uint8_t sense[ST_TOUCH_SENSE_SIZE];
#endif
} __packed;

/* What will be sent to USB interface. */
struct st_tp_usb_frame {
#define USB_FRAME_FLAGS_BUTTON	(1 << 0)
	/*
	 * This will be true if user clicked on touchpad.
	 * TODO(stimim): add corresponding code for button signal.
	 */
	uint8_t flags;

	/* This will be `st_tp_frame.frame` but each pixel will be scaled to 8
	 * bits value. */
	uint8_t frame[ST_TOUCH_ROWS * ST_TOUCH_COLS];
} __packed;

/* next buffer index SPI will write to. */
static volatile uint32_t spi_buffer_index = 0;
/* next buffer index USB will read from */
static volatile uint32_t usb_buffer_index = 0;
static struct st_tp_usb_frame frame_buffer[2]; /* double buffering */

static int st_tp_read_frame(void);
static int st_tp_send_ack(void);

static int debug_mode = 0;

static void print_frame(void)
{
	static char debug_line[ST_TOUCH_COLS + 5];
	int i, j, index;
	int v;
	struct st_tp_usb_frame *buffer = &frame_buffer[usb_buffer_index & 1];

	if (usb_buffer_index == spi_buffer_index)
		/* buffer is empty. */
		return;

	/* We will have ~150 FPS, let's print ~4 frames per second */
	if (usb_buffer_index % 37 == 0) {
		/* move cursor back to top left corner */
		CPRINTF("\x1b[H");
		CPUTS("==============\n");
		for (i = 0; i < ST_TOUCH_ROWS; i++) {
			for (j = 0; j < ST_TOUCH_COLS; j++) {
				index = i * ST_TOUCH_COLS;
				index += (ST_TOUCH_COLS - j - 1); // flip X
				v = buffer->frame[index];

				if (v > 0)
					debug_line[j] = '0' + v * 10 / 256;
				else
					debug_line[j] = ' ';
			}
			debug_line[j++] = '\n';
			debug_line[j++] = '\0';
			CPRINTF(debug_line);
		}
		CPUTS("==============\n");
	}
}

/* For testing, computes X/Y coordinates by centor of mess */
static void compute_and_set_touchpad_report(void)
{
	uint64_t sum_x = 0, sum_y = 0;
	uint32_t i, j, index, n = 0;
	struct usb_hid_touchpad_report report;
	struct st_tp_usb_frame *buffer = &frame_buffer[usb_buffer_index & 1];
	static uint8_t finger_status = 0;
	const uint32_t W = 3000, H = 1500;

	if (usb_buffer_index == spi_buffer_index)
		/* buffer is empty. */
		return;

	memset(&report, 0, sizeof(report));
	report.id = 0x01;
	report.timestamp =
		__hw_clock_source_read() / USB_HID_TOUCHPAD_TIMESTAMP_UNIT;
	report.count = 0;

	for (i = 0; i < ST_TOUCH_ROWS; i++) {
		for (j = 0; j < ST_TOUCH_COLS; j++) {
			index = i * ST_TOUCH_COLS + j;

			if (buffer->frame[index] < 25)
				continue;
			/*
			 * max(sum_?) = 25 * 18 * 255 * 25 * 3000 = 8606250000
			 */
			sum_x += buffer->frame[index] * (j * W / ST_TOUCH_COLS);
			sum_y += buffer->frame[index] * (i * H / ST_TOUCH_ROWS);
			/* max(n) = 25 * 18 * 255 */
			n += buffer->frame[index];
		}
	}

	if (n) {
		report.finger[0].id = 1;
		report.finger[0].tip = 1;
		report.finger[0].inrange = 1;
		report.finger[0].pressure = 50;
		/* width and height of finger, not touchpad... */
		report.finger[0].width = 10;
		report.finger[0].height = 10;
		report.finger[0].x = W - MIN(sum_x / n, W);
		report.finger[0].y = MIN(sum_y / n, H);
		report.count = 1;
		report.button = 0;
		finger_status = 1;
	} else if (finger_status) {
		report.finger[0].id = 1;
		report.count = 1;
		finger_status = 0;
	}

	if (report.count) {
		set_touchpad_report(&report);
	}
}

static int st_tp_read_report(void)
{
	/* because we are using double buffering, so, if usb_buffer_index = N
	 * 1. spi_buffer_index == N      => ok, both slot is empty
	 * 2. spi_buffer_index == N + 1  => ok, second slot is empty
	 * 3. spi_buffer_index == N + 2  => not ok, need to wait for USB.
	 *
	 * TODO(stimim): can we override second slot for case (3)?
	 */
	if ((uint32_t)(spi_buffer_index - usb_buffer_index) <= 1) {
		if (st_tp_read_frame() == EC_SUCCESS)
			atomic_add(&spi_buffer_index, 1);
	}
	st_tp_send_ack();

	if (debug_mode) {
		compute_and_set_touchpad_report();
		print_frame();
		atomic_add(&usb_buffer_index, 1);
	}
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
	/*
	 * sizeof(tmp_buf) > 640 (the stack size), have to make it static
	 * variable.
	 */
	static struct st_tp_frame tmp_buf;
	int ret = EC_SUCCESS;

	/*
	 * theoretically, we should read host buffer header to check if data is
	 * valid, but the data should always be ready when interrupt pin is low.
	 * Let's skip this check for now.
	 */
	ret = st_tp_command_response(
			ST_TP_CMD_READ_SPI_HOST_BUFFER,
			ST_TP_HEAT_MAP_ADDR,
			&tmp_buf,
			sizeof(struct st_tp_frame));
	if (ret == EC_SUCCESS) {
#if 0
		/* If BYTES_PER_FRAME = 1, then we can memcpy directly.
		 * This takes about 0.1ms per frame. */
		memcpy(dest, tmp_buf.frame, ST_TOUCH_COLS * ST_TOUCH_ROWS);
#else
		/* Down scaling and move data into frame_buffer, this takes
		 * about 0.35ms per frame */
		int i;
		short v;
		uint8_t *dest = frame_buffer[spi_buffer_index & 1].frame;
		for (i = 0; i < ST_TOUCH_COLS * ST_TOUCH_ROWS; i++) {
			v = (tmp_buf.frame[i * 2] |
			     (tmp_buf.frame[i * 2 + 1] << 8));
			v = MAX(0, v);
			v = MIN(v >> (BITS_PER_PIXEL - 8), 255);
			dest[i] = v;
		}
#endif
	}
	return ret;
}

static int st_tp_send_ack(void)
{
	uint8_t tx_buf[1] = {ST_TP_CMD_SPI_HOST_BUFFER_ACK};
	return spi_transaction(SPI, tx_buf, sizeof(tx_buf), NULL, 0);
}

static int st_tp_start_scan(void)
{
	uint8_t tx_buf[] = {
		ST_TP_CMD_WRITE_SCAN_MODE_SELECT,
		ST_TP_SCAN_MODE_ACTIVE,
		0x01,  /* Enable multi-touch */
	};
	return spi_transaction(SPI, tx_buf, sizeof(tx_buf), NULL, 0);
}

static int st_tp_stop_scan(void)
{
	uint8_t tx_buf[] = {
		ST_TP_CMD_WRITE_SCAN_MODE_SELECT,
		ST_TP_SCAN_MODE_ACTIVE,
		0x00,  /* Low power mode */
	};
	return spi_transaction(SPI, tx_buf, sizeof(tx_buf), NULL, 0);
}

static int st_tp_load_system_info(void)
{
	uint8_t tx_buf[] = {ST_TP_CMD_WRITE_SYSTEM_COMMAND, 0x06, 0x01};
	uint8_t _rx_buf[5];  /* 1 dummy byte */
	uint8_t *rx_buf = _rx_buf + 1;
	int retry;
	uint16_t count;
	int ret;

	st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
			       _rx_buf, sizeof(_rx_buf));
	count = rx_buf[2] | (rx_buf[3] << 8);
	ret = spi_transaction(SPI, tx_buf, sizeof(tx_buf), NULL, 0);
	if (ret)
		return ret;

	ret = EC_ERROR_TIMEOUT;
	retry = 5;
	while (retry--) {
		st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
				       _rx_buf, sizeof(_rx_buf));
		if (rx_buf[0] == 0xA5 && rx_buf[1] == 0x01 &&
		    count != (rx_buf[2] | (rx_buf[3] << 8))) {
			ret = EC_SUCCESS;
			break;
		}
		udelay(10 * MSEC);
	}
	return ret;
}

static int st_tp_read_system_info(int load)
{
	int ret;
	uint8_t _rx_buf[8 + 1];
	uint8_t *rx_buf = _rx_buf + 1;  /* 1 dummy byte */

	if (load)
		st_tp_load_system_info();
	ret = st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
				     _rx_buf, 4 + 1);
	CPRINTS("header: %02x %02x %02x %02x",
		rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
	ret = st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0008,
				     _rx_buf, sizeof(_rx_buf));
	CPRINTS("chip0: %02x %02x %02x %02x",
		rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
	CPRINTS("chip1: %02x %02x %02x %02x",
		rx_buf[4], rx_buf[5], rx_buf[6], rx_buf[7]);
	ret = st_tp_command_response(ST_TP_CMD_READ_FW_CONFIG, 0x0030,
				     _rx_buf, 3);
	CPRINTS("sense len: %02x %02x ", rx_buf[0], rx_buf[1]);

	return ret;
}

int st_tp_read_all_events(void)
{
	/* each event is 8 bytes, there are 32 events */
	uint8_t rx_buf[8 * 32], cmd = ST_TP_CMD_READ_ALL_EVENTS;
	int ret, i;
	ret = spi_transaction(SPI, &cmd, 1, rx_buf, sizeof(rx_buf));

	for (i = 0; i < 32; i++) {
		/* whatever... */
		;
	}
	return ret;
}

void st_tp_reset_by_pin(void)
{
	/* Reset touchpad by power pin.
	 * TODO(stimim): perhaps we can use reset pin in the future?
	 */
	gpio_set_level(GPIO_EN_PP3300_TP, 0);
	udelay(10 * MSEC);
	gpio_set_level(GPIO_EN_PP3300_TP, 1);
	udelay(100 * MSEC);
	gpio_set_level(GPIO_EN_PP3300_TP, 0);
	udelay(300 * MSEC);
}

static int st_tp_reset(void)
{
	st_tp_reset_by_pin();
	return st_tp_read_all_events();
}

/* Initialize the controller ICs after reset */
static void st_tp_init(void)
{
	st_tp_reset();
	/* System info will be loaded by default */
	st_tp_read_system_info(0);
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

		while (!gpio_get_level(GPIO_TOUCHPAD_INT))
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
	struct st_tp_usb_frame *buffer = &frame_buffer[usb_buffer_index & 1];

	if (debug_mode) /* frames will be printed on console */
		return 0;

	if (usb_buffer_index == spi_buffer_index)
		/* buffer is empty */
		return 0;

	num_byte_available = sizeof(*buffer) - transmit_report_offset;
	if (num_byte_available > 0) {
		if (transmit_report_offset == 0)
			packet_header.flags |= HEADER_FLAGS_NEW_FRAME;
		memcpy_to_usbram((void *)ptr,
				 &packet_header,
				 sizeof(packet_header));
		packet_header.index++;
		count += sizeof(packet_header);
		num_byte_available = MIN(tx_size - count, num_byte_available);
		memcpy_to_usbram((void *)(ptr + count),
				 (((uint8_t *)buffer) + transmit_report_offset),
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

static void st_tp_enable_interrupt(int enable)
{
	uint8_t tx_buf[] = {
		ST_TP_CMD_WRITE_SYSTEM_COMMAND, 0x01, enable ? 1 : 0};
	spi_transaction(SPI, tx_buf, sizeof(tx_buf), NULL, 0);
}

static void st_tp_usb_enable(void)
{
	st_tp_start_scan();
	st_tp_send_ack();
	CPRINTS("%s:enable interrupt", __func__);
	gpio_enable_interrupt(GPIO_TOUCHPAD_INT);
	st_tp_enable_interrupt(1);
}
DECLARE_DEFERRED(st_tp_usb_enable);

static void st_tp_usb_disable(void)
{
	st_tp_stop_scan();
	CPRINTS("%s:disable interrupt", __func__);
	st_tp_enable_interrupt(0);
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


/* Debugging commands */
static int command_touchpad_st(int argc, char **argv)
{
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;
	if (strcasecmp(argv[1], "enable") == 0) {
		debug_mode = 1;
		hook_call_deferred(&st_tp_usb_enable_data, 0);
		return 0;
	} else if (strcasecmp(argv[1], "disable") == 0) {
		debug_mode = 0;
		hook_call_deferred(&st_tp_usb_disable_data, 0);
		return 0;
	} else if (strcasecmp(argv[1], "version") == 0) {
		st_tp_read_system_info(1);
		return 0;
	} else {
		return EC_ERROR_PARAM1;
	}
}
DECLARE_CONSOLE_COMMAND(touchpad_st, command_touchpad_st,
			"<enable|disable|version>",
			"Read write spi. id is spi_devices array index");
