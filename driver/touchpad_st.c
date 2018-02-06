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
#include "touchpad_st.h"
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

struct st_tp_system_info_t system_info;

struct packet_header_t {
	uint8_t index;

#define HEADER_FLAGS_NEW_FRAME	(1 << 0)
	uint8_t flags;
} __packed;

static struct packet_header_t packet_header = {
	.index = 0,
	.flags = 0,
};

/* What will be sent to USB interface. */
struct st_tp_usb_packet_t {
#define USB_FRAME_FLAGS_BUTTON	(1 << 0)
	/*
	 * This will be true if user clicked on touchpad.
	 * TODO(stimim): add corresponding code for button signal.
	 */
	uint8_t flags;

	/* This will be `st_tp_host_buffer_heat_map_t.frame` but each pixel
	 * will be scaled to 8 bits value. */
	uint8_t frame[ST_TOUCH_ROWS * ST_TOUCH_COLS];
} __packed;

/* next buffer index SPI will write to. */
static volatile uint32_t spi_buffer_index = 0;
/* next buffer index USB will read from */
static volatile uint32_t usb_buffer_index = 0;
static struct st_tp_usb_packet_t usb_packet[2]; /* double buffering */

static int st_tp_read_frame(void);
static int st_tp_send_ack(void);
static int get_heat_map_addr(void) __attribute__((pure));

static int debug_mode = 0;

static struct {
#ifdef ST_TP_DUMMY_BYTE
	uint8_t dummy;
#endif
	union {
		uint8_t bytes[512];
		struct st_tp_host_buffer_heat_map_t heat_map;
		struct st_tp_host_data_header_t header;
	} /* anonymous */;
} __packed rx_buf;

static int get_heat_map_addr(void)
{
	switch (system_info.release_info) {
		case 0x1:
			return 0x20;
		default:
			return -1; /* Unknown version */
	}
}

static void print_frame(void)
{
	static char debug_line[ST_TOUCH_COLS + 5];
	int i, j, index;
	int v;
	struct st_tp_usb_packet_t *packet = &usb_packet[usb_buffer_index & 1];

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
				v = packet->frame[index];

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
	struct st_tp_host_buffer_heat_map_t *heat_map = &rx_buf.heat_map;
	int ret = EC_SUCCESS;
	int rx_len = sizeof(*heat_map) + ST_TP_DUMMY_BYTE;

	if (get_heat_map_addr() < 0)
		goto failed;
	/*
	 * theoretically, we should read host buffer header to check if data is
	 * valid, but the data should always be ready when interrupt pin is low.
	 * Let's skip this check for now.
	 */
	ret = st_tp_command_response(
			ST_TP_CMD_READ_SPI_HOST_BUFFER,
			get_heat_map_addr(),
			&rx_buf,
			rx_len);
	if (ret == EC_SUCCESS) {
#if 0
		/* If BYTES_PER_FRAME = 1, then we can memcpy directly.
		 * This takes about 0.1ms per frame. */
		memcpy(dest, heat_map->frame, ST_TOUCH_COLS * ST_TOUCH_ROWS);
#else
		/* Down scaling and move data into usb_packet, this takes
		 * about 0.35ms per frame */
		int i;
		short v;
		uint8_t *dest = usb_packet[spi_buffer_index & 1].frame;
		for (i = 0; i < ST_TOUCH_COLS * ST_TOUCH_ROWS; i++) {
			v = (heat_map->frame[i * 2] |
			     (heat_map->frame[i * 2 + 1] << 8));
			v = MAX(0, v);
			v = MIN(v >> (BITS_PER_PIXEL - 8), 255);
			dest[i] = v;
		}
#endif
	}
failed:
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

static int st_tp_load_host_data(uint8_t mem_id)
{
	uint8_t tx_buf[] = {
		ST_TP_CMD_WRITE_SYSTEM_COMMAND, 0x06, mem_id,
	};
	int retry, ret;
	uint16_t count;
	struct st_tp_host_data_header_t *header = &rx_buf.header;
	int rx_len = sizeof(*header) + ST_TP_DUMMY_BYTE;

	st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
			       &rx_buf, rx_len);
	if (header->host_data_mem_id == mem_id)
		return EC_SUCCESS; /* already loaded no need to reload */

	count = header->count;

	ret = spi_transaction(SPI, tx_buf, sizeof(tx_buf), NULL, 0);
	if (ret)
		return ret;

	ret = EC_ERROR_TIMEOUT;
	retry = 5;
	while (retry--) {
		st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
				       &rx_buf, rx_len);
		if (header->magic == ST_TP_HEADER_MAGIC &&
		    header->host_data_mem_id == mem_id &&
		    header->count != count) {
			ret = EC_SUCCESS;
			break;
		}
		udelay(10 * MSEC);
	}
	return ret;
}

static int st_tp_read_system_info(int load)
{
	int ret, size;
	int rx_len = ST_TP_DUMMY_BYTE + ST_TP_SYSTEM_INFO_LEN;
	uint8_t *ptr = rx_buf.bytes;

	if (load)
		st_tp_load_host_data(ST_TP_MEM_ID_SYSTEM_INFO);
	ret = st_tp_command_response(ST_TP_CMD_READ_HOST_DATA_MEMORY, 0x0000,
				     &rx_buf, rx_len);
	if (ret)
		return ret;

	/* Parse the content */
	size = sizeof(system_info.header)
		+ sizeof(system_info.api_ver_rev)
		+ sizeof(system_info.api_ver_minor)
		+ sizeof(system_info.api_ver_major)
		+ sizeof(system_info.chip0_ver)
		+ sizeof(system_info.chip0_id)
		+ sizeof(system_info.chip1_ver)
		+ sizeof(system_info.chip1_id)
		+ sizeof(system_info.fw_ver)
		+ sizeof(system_info.svn_rev)
		+ sizeof(system_info.cfg_ver)
		+ sizeof(system_info.cfg_project_id)
		+ sizeof(system_info.cx_ver)
		+ sizeof(system_info.cx_project_id)
		+ sizeof(system_info.cfg_afe_ver)
		+ sizeof(system_info.cx_afe_ver)
		+ sizeof(system_info.panel_cfg_afe_ver)
		+ sizeof(system_info.protocol)
		+ sizeof(system_info.die_id)
		+ sizeof(system_info.release_info)
		+ sizeof(system_info.fw_crc)
		+ sizeof(system_info.cfg_crc);
	memcpy(&system_info, ptr, size);

	/* Check header */
	if (system_info.header.magic != ST_TP_HEADER_MAGIC ||
	    system_info.header.host_data_mem_id != ST_TP_MEM_ID_SYSTEM_INFO)
		return EC_ERROR_UNKNOWN;

	ptr += size;
	ptr += 16;

	size = sizeof(system_info.scr_res_x)
		+ sizeof(system_info.scr_res_y)
		+ sizeof(system_info.scr_tx_len)
		+ sizeof(system_info.scr_rx_len)
		+ sizeof(system_info.key_len)
		+ sizeof(system_info.frc_len);
	memcpy(&system_info.scr_res_x, ptr, size);
	ptr += size;
	ptr += 40;

	size = sizeof(system_info.dbg_frame_addr);
	memcpy(&system_info.dbg_frame_addr, ptr, size);
	ptr += size;
	ptr += 6;

	size = sizeof(system_info.ms_scr_raw_addr)
		+ sizeof(system_info.ms_scr_filter_addr)
		+ sizeof(system_info.ms_scr_str_addr)
		+ sizeof(system_info.ms_scr_bl_addr)
		+ sizeof(system_info.ss_tch_tx_raw_addr)
		+ sizeof(system_info.ss_tch_tx_filter_addr)
		+ sizeof(system_info.ss_tch_tx_str_addr)
		+ sizeof(system_info.ss_tch_tx_bl_addr)
		+ sizeof(system_info.ss_tch_rx_raw_addr)
		+ sizeof(system_info.ss_tch_rx_filter_addr)
		+ sizeof(system_info.ss_tch_rx_str_addr)
		+ sizeof(system_info.ss_tch_rx_bl_addr)
		+ sizeof(system_info.key_raw_addr)
		+ sizeof(system_info.key_filter_addr)
		+ sizeof(system_info.key_str_addr)
		+ sizeof(system_info.key_bl_addr)
		+ sizeof(system_info.frc_raw_addr)
		+ sizeof(system_info.frc_filter_addr)
		+ sizeof(system_info.frc_str_addr)
		+ sizeof(system_info.frc_bl_addr)
		+ sizeof(system_info.ss_hvr_tx_raw_addr)
		+ sizeof(system_info.ss_hvr_tx_filter_addr)
		+ sizeof(system_info.ss_hvr_tx_str_addr)
		+ sizeof(system_info.ss_hvr_tx_bl_addr)
		+ sizeof(system_info.ss_hvr_rx_raw_addr)
		+ sizeof(system_info.ss_hvr_rx_filter_addr)
		+ sizeof(system_info.ss_hvr_rx_str_addr)
		+ sizeof(system_info.ss_hvr_rx_bl_addr)
		+ sizeof(system_info.ss_prx_tx_raw_addr)
		+ sizeof(system_info.ss_prx_tx_filter_addr)
		+ sizeof(system_info.ss_prx_tx_str_addr)
		+ sizeof(system_info.ss_prx_tx_bl_addr)
		+ sizeof(system_info.ss_prx_rx_raw_addr)
		+ sizeof(system_info.ss_prx_rx_filter_addr)
		+ sizeof(system_info.ss_prx_rx_str_addr)
		+ sizeof(system_info.ss_prx_rx_bl_addr);
	memcpy(&system_info.ms_scr_raw_addr, ptr, size);
	ptr += size;

#define ST_TP_SHOW(attr) CPRINTS(#attr ": %04x", system_info.attr)
	ST_TP_SHOW(chip0_id);
	ST_TP_SHOW(chip0_ver);
	ST_TP_SHOW(scr_tx_len);
	ST_TP_SHOW(scr_rx_len);
	ST_TP_SHOW(release_info);
#undef SHOW
	return ret;
}

int st_tp_read_all_events(void)
{
	/* each event is 8 bytes, there are 32 events */
	uint8_t cmd = ST_TP_CMD_READ_ALL_EVENTS;
	int ret, i, rx_len = 8 * 32;
	ret = spi_transaction(SPI, &cmd, 1, (uint8_t *)&rx_buf, rx_len);

	for (i = 0; i < 32; i++) {
		/* whatever... */
		;
	}
	return ret;
}

void st_tp_reset_by_pin(void)
{
	board_touchpad_reset();
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
	struct st_tp_usb_packet_t *packet = &usb_packet[usb_buffer_index & 1];

	if (debug_mode) /* frames will be printed on console */
		return 0;

	if (usb_buffer_index == spi_buffer_index)
		/* buffer is empty */
		return 0;

	num_byte_available = sizeof(*packet) - transmit_report_offset;
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
				 (((uint8_t *)packet) + transmit_report_offset),
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
