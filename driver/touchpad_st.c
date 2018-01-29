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

#define ST_TP_VER	2

#if (ST_TP_VER == 2)
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

#define ST_TP_HEAT_MAP_ADDR 0x0020

#define ST_TP_SCAN_MODE_ACTIVE		0x00
#define ST_TP_SCAN_MODE_LOW_POWER	0x01
#define ST_TP_SCAN_MODE_TUNING_WIZARD	0x02
#define ST_TP_SCAN_MODE_LOCKED		0x03

#define ST_TOUCH_ROWS		(18)  /* force len */
#define ST_TOUCH_COLS		(25)  /* sense len */

#elif (ST_TP_VER == 1)

#define ST_TP_CMD_SPI_HOST_BUFFER_ACK		0xAC
#define ST_TP_CMD_READ_SPI_HOST_BUFFER		0xD0
#define ST_TP_HEAT_MAP_ADDR 0x5020

#define ST_TOUCH_ROWS		24 /* sense len */
#define ST_TOUCH_COLS		14 /* force len */

#else  /* ST_TP_VER */
#error "ST_TP_VER should be 1 or 2"
#endif  /* ST_TP_VER */

#define ST_TOUCH_HEADER_SIZE	32
#define BYTES_PER_PIXEL		2
/* TODO(stimim): this will become one byte per pixel in the future */
#define ST_TOUCH_FRAME_SIZE	(ST_TOUCH_ROWS * ST_TOUCH_COLS * BYTES_PER_PIXEL)
#define ST_TOUCH_FORCE_SIZE	(ST_TOUCH_ROWS * BYTES_PER_PIXEL)
#define ST_TOUCH_SENSE_SIZE	(ST_TOUCH_COLS * BYTES_PER_PIXEL)

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

/* What will be sent to USB interface. */
struct __attribute__((packed)) st_tp_usb_frame {
	/* This will be `st_tp_frame.frame`. */
	uint8_t frame[ST_TOUCH_FRAME_SIZE];
	/* This will be true if user clicked on touchpad.
	 * TODO(stimim): add corresponding code for button signal.
	 */
	uint8_t button:1;
};

/* next buffer index SPI will write to. */
static volatile uint32_t spi_buffer_index = 0;
/* next buffer index USB will read from */
static volatile uint32_t usb_buffer_index = 0;

static int st_tp_read_frame(void);
static int st_tp_send_ack(void);

static int debug_mode = 0;

static void print_frame(void)
{
	static char debug_line[ST_TOUCH_COLS + 5];
	int i, j, k, index;
	short v;
	struct st_tp_frame *buffer = &frame_buffer[usb_buffer_index % 2];
	const uint32_t MAX_V = 1<<11;

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
				index = i * ST_TOUCH_COLS + j;
				index *= BYTES_PER_PIXEL;

				for (v = k = 0; k < BYTES_PER_PIXEL; k++)
					v |= buffer->frame[index + k] << (k * 8);

				if (v > 0)
					debug_line[j] = '0' + v * 10 / MAX_V;
				else
					debug_line[j] = ' ';
			}
			debug_line[j++] = '\n';
			debug_line[j++] = '\0';
			CPRINTF(debug_line);
		}
		CPUTS("==============\n");
	}
	atomic_add(&usb_buffer_index, 1);
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

	if (debug_mode)
		print_frame();
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
	uint8_t rx_buf[2];
	st_tp_command_response(ST_TP_CMD_READ_SPI_HOST_BUFFER, 0x0000,
			       rx_buf, 2);
#if 0
	rx_buf[1] & 0x01;  // data_valid
	rx_buf[1] & 0x02;  // evt_fifo_not_empty
	rx_buf[1] & 0x04;  // sys_fault
	rx_buf[1] & 0x08;  // heat_map_mt_rdy
	rx_buf[1] & 0x10;  // heat_map_sf_rdy
	rx_buf[1] & 0x20;  // heat_map_ss_rdy
#endif
	if ((rx_buf[1] & 0x01) || (rx_buf[1] & 0x08))
		/* mutual heat map is available */
		return st_tp_command_response(
				ST_TP_CMD_READ_SPI_HOST_BUFFER,
				ST_TP_HEAT_MAP_ADDR,
				&frame_buffer[spi_buffer_index & 1],
				sizeof(struct st_tp_frame));
	return EC_ERROR_BUSY;
}

static int st_tp_send_ack(void)
{
#if (ST_TP_VER == 2)
	uint8_t tx_buf[1] = {ST_TP_CMD_SPI_HOST_BUFFER_ACK};
#elif (ST_TP_VER == 1)
	uint8_t tx_buf[2] = {ST_TP_CMD_SPI_HOST_BUFFER_ACK, 0x00};
#endif
	return spi_transaction(SPI, tx_buf, ARRAY_SIZE(tx_buf), NULL, 0);
}

static int st_tp_start_scan(void)
{
#if (ST_TP_VER == 2)
	uint8_t tx_buf[] = {
		ST_TP_CMD_WRITE_SCAN_MODE_SELECT,
		ST_TP_SCAN_MODE_ACTIVE,
		0x01,  /* Enable multi-touch */
	};
#elif (ST_TP_VER == 1)
	uint8_t tx_buf[2] = {0x93, 0x00};
#endif
	return spi_transaction(SPI, tx_buf, ARRAY_SIZE(tx_buf), NULL, 0);
}

static int st_tp_stop_scan(void)
{
#if (ST_TP_VER == 2)
	uint8_t tx_buf[] = {
		ST_TP_CMD_WRITE_SCAN_MODE_SELECT,
		ST_TP_SCAN_MODE_LOCKED,
		0x02,  /* Idle */
	};
#elif (ST_TP_VER == 1)
	uint8_t tx_buf[2] = {0x92, 0x00};
#endif
	return spi_transaction(SPI, tx_buf, ARRAY_SIZE(tx_buf), NULL, 0);
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
			       _rx_buf, ARRAY_SIZE(_rx_buf));
	count = rx_buf[2] | (rx_buf[3] << 8);
	ret = spi_transaction(SPI, tx_buf, ARRAY_SIZE(tx_buf), NULL, 0);
	if (ret)
		return ret;

	ret = EC_ERROR_TIMEOUT;
	for (retry = 0; retry < 5; retry++) {
		st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
				       _rx_buf, ARRAY_SIZE(_rx_buf));
		if (rx_buf[0] != 0xA5)
			goto next;
		if (rx_buf[1] != 0x01)
			goto next;
		if (count != (rx_buf[2] | (rx_buf[3] << 8))) {
			ret = EC_SUCCESS;
			break;
		}
next:
		udelay(10 * MSEC);
	}
	return ret;
}

static int st_tp_read_system_info(int load)
{
	int ret;
#if (ST_TP_VER == 2)
	uint8_t _rx_buf[8 + 1];
	uint8_t *rx_buf = _rx_buf + 1;  /* 1 dummy byte */

	if (load)
		st_tp_load_system_info();
	ret = st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
				     _rx_buf, 4 + 1);
	CPRINTS("header: %02x %02x %02x %02x",
		rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
	ret = st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0008,
				     _rx_buf, ARRAY_SIZE(_rx_buf));
	CPRINTS("chip0: %02x %02x %02x %02x",
		rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
	CPRINTS("chip1: %02x %02x %02x %02x",
		rx_buf[4], rx_buf[5], rx_buf[6], rx_buf[7]);
	ret = st_tp_command_response(ST_TP_CMD_READ_FW_CONFIG, 0x0030,
				     _rx_buf, 3);
	CPRINTS("sense len: %02x %02x ", rx_buf[0], rx_buf[1]);

#elif (ST_TP_VER == 1)
	uint8_t hwid[7] = {0};

	ret = st_tp_command_response(0xB4, 0x0007, hwid, 7);
	CPRINTS("chip id: %02x %02x %02x %02x %02x %02x %02x",
		hwid[0], hwid[1], hwid[2], hwid[3], hwid[4], hwid[5], hwid[6]);
#endif
	return ret;
}

int st_tp_read_one_event(uint8_t *buf)
{
#if ST_TP_VER == 2
	/* Does nothing */
#elif ST_TP_VER == 1
	uint8_t cmd_read_one_event = 0x85;
	int ret = spi_transaction(SPI, &cmd_read_one_event, 1,
				  buf, 9);
	if (ret != EC_SUCCESS) {
		return ret;
	}
#endif
	return 0;
}

int st_tp_read_all_events(void)
{
#if ST_TP_VER == 2
	/* each event is 8 bytes, there are 32 events */
	uint8_t rx_buf[8 * 32], cmd = ST_TP_CMD_READ_ALL_EVENTS;
	int ret, i;
	ret = spi_transaction(SPI, &cmd, 1, rx_buf, ARRAY_SIZE(rx_buf));

	for (i = 0; i < 32; i++) {
		/* whatever... */
		;
	}
	return ret;
#elif ST_TP_VER == 1
	uint8_t rx_buf[32];
	int i, ret;
	for (i = 32; i < 32; i++) {
		ret = st_tp_read_one_event(rx_buf);
		if (ret == 0 && rx_buf[1] == 0x10)
			break;
		msleep(10);
	}
#endif

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
	struct st_tp_frame *buffer = &frame_buffer[usb_buffer_index & 1];

	if (debug_mode) /* frames will be printed on console */
		return 0;

	if (usb_buffer_index == spi_buffer_index)
		/* buffer is empty */
		return 0;

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
				 (buffer->frame + transmit_report_offset),
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
	spi_transaction(SPI, tx_buf, ARRAY_SIZE(tx_buf), NULL, 0);
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
	} else if (strcasecmp(argv[1], "clear_int") == 0) {
		while (!gpio_get_level(GPIO_TOUCHPAD_INT))
			st_tp_send_ack();
		return 0;
	} else {
		return EC_ERROR_PARAM1;
	}
}
DECLARE_CONSOLE_COMMAND(touchpad_st, command_touchpad_st,
			"<enable|disable|version>",
			"Read write spi. id is spi_devices array index");
