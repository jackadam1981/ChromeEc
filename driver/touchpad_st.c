/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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

/* Console output macros */
#define CC_TOUCHPAD CC_USB
#define CPUTS(outstr) cputs(CC_TOUCHPAD, outstr)
#define CPRINTF(format, args...) cprintf(CC_TOUCHPAD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_TOUCHPAD, format, ## args)

uint8_t spi_buffer[32];

/*
 * Timestamp of last interrupt (32 bits are enough as we divide the value by 100
 * and then put it in a 16-bit field).
 */
static uint32_t irq_ts;

static int st_tp_read_report(void)
{
	int ri;
	struct usb_hid_touchpad_report report;
	uint16_t timestamp;

	/* Compute and save timestamp early in case another interrupt comes. */
	timestamp = irq_ts / USB_HID_TOUCHPAD_TIMESTAMP_UNIT;

	memset(&report, 0, sizeof(report));
	report.id = 0x01;
	ri = 0; /* Next finger index in HID report */

	report.count = ri;
	report.timestamp = timestamp;

	set_touchpad_report(&report);

	return 0;
}

/* Send a command to device and read response back */
static int st_tp_command_response(const struct spi_device_t *spi,
				  uint8_t cmd, uint16_t addr,
				  void *rx_buf, int len)
{
	uint8_t tx_buf[] = { cmd, addr >> 8, addr & 0xFF, };
	return spi_transaction(spi, tx_buf, sizeof(tx_buf), rx_buf, len);
}

static int st_tp_read_chip_id(void)
{
	const struct spi_device_t *spi = &spi_devices[SPI_ST_TP_DEVICE_ID];
	int ret, i;
	uint8_t hwid[8];
	for (i = 0; i < 7; ++ i) {
		hwid[i] = 0;
	}
	ret = st_tp_command_response(spi, 0xB4, 0x0007, hwid, 7);
	CPRINTF("%s: ret = %d\n", __func__, ret);
	for (i = 0; i < 7; ++ i) {
		CPRINTF("%02x\n", hwid[i]);
	}
	return ret;
}

int st_tp_read_one_event(void)
{
	const struct spi_device_t *spi = &spi_devices[SPI_ST_TP_DEVICE_ID];
	uint8_t cmd_read_one_event = 0x85;
	int ret = spi_transaction(spi, &cmd_read_one_event, 1,
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
	irq_ts = __hw_clock_source_read();

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
