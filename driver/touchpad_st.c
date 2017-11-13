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
#include "task.h"
#include "timer.h"
#include "touchpad.h"
#include "update_fw.h"
#include "util.h"
#include "usb_hid_touchpad.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_TOUCHPAD, outstr)
#define CPRINTF(format, args...) cprintf(CC_TOUCHPAD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_TOUCHPAD, format, ## args)

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

/* Initialize the controller ICs after reset */
static void st_tp_init(void)
{
	int rv = 0;

	CPRINTS("%s", __func__);

	CPRINTS("%s:%d", __func__, rv);
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
