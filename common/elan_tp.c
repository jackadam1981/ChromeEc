/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "elan_tp.h"
#include "gpio.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* FIXME */
#define CC_TOUCHPAD CC_SYSTEM
#define I2C_PORT_TOUCHPAD 0
#define I2C_ADDR_TOUCHPAD (0x15 << 1)

/* Console output macros */
#define CPUTS(outstr) cputs(CC_TOUCHPAD, outstr)
#define CPRINTF(format, args...) cprintf(CC_TOUCHPAD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_TOUCHPAD, format, ## args)

/******************************************************************************/
/* How to talk to the controller */
/******************************************************************************/

#define ETP_I2C_RESET			0x0100
#define ETP_I2C_WAKE_UP			0x0800
#define ETP_I2C_SLEEP			0x0801
#define ETP_I2C_DESC_CMD		0x0001
#define ETP_I2C_REPORT_DESC_CMD		0x0002
#define ETP_I2C_STAND_CMD		0x0005
#define ETP_I2C_SET_CMD			0x0300

#define ETP_ENABLE_ABS		0x0001

#define ETP_I2C_REPORT_LEN		34
#define ETP_I2C_INF_LENGTH		2
#define ETP_I2C_DESC_LENGTH		30
#define ETP_I2C_REPORT_DESC_LENGTH	158

#define ETP_MAX_FINGERS		5
#define ETP_FINGER_DATA_LEN	5

#define ETP_REPORT_ID		0x5D
#define ETP_REPORT_ID_OFFSET	2
#define ETP_TOUCH_INFO_OFFSET	3
#define ETP_FINGER_DATA_OFFSET	4
#define ETP_HOVER_INFO_OFFSET	30
#define ETP_MAX_REPORT_LEN	34

static int elan_tp_read_block(uint16_t reg, uint8_t *val, uint16_t len)
{
	uint8_t buf[2];
	int rv;

	buf[0] = reg;
	buf[1] = reg >> 8;

	rv = i2c_xfer(I2C_PORT_TOUCHPAD, I2C_ADDR_TOUCHPAD, buf, sizeof(buf),
		      val, len, I2C_XFER_SINGLE);
	if (rv)
		CPRINTF("[%T read block error]\n");

	return rv;
}

static int elan_tp_write_cmd(uint16_t reg, uint16_t val)
{
	uint8_t buf[4];
	int rv;

	buf[0] = reg;
	buf[1] = reg >> 8;
	buf[2] = val;
	buf[3] = val >> 8;
	rv = i2c_xfer(I2C_PORT_TOUCHPAD, I2C_ADDR_TOUCHPAD, buf, sizeof(buf),
		      0, 0, I2C_XFER_SINGLE);
	if (rv)
		CPRINTF("[%T write error]\n");

	return rv;
}

/* FIXME */
uint8_t tp_buf[256];

static int elan_tp_read_report(void)
{
	int rv;

	rv = i2c_xfer(I2C_PORT_TOUCHPAD, I2C_ADDR_TOUCHPAD, 0, 0, tp_buf,
		      ETP_I2C_REPORT_LEN, I2C_XFER_SINGLE);
	if (rv) {
		CPRINTF("[%T read report error]\n");
	} else {
		int i;
		int touch_info = tp_buf[ETP_TOUCH_INFO_OFFSET];
		uint8_t *finger = tp_buf+ETP_FINGER_DATA_OFFSET;
		/* FIXME: Check report id == 0x5d */
		/* FIXME: Add hover support */
		CPRINTF("[%T ");
#if 0
		for (i = 0; i < ETP_I2C_REPORT_LEN; i++)
			CPRINTF("%02x", tp_buf[i]);
		CPRINTF(" || ");
#endif
		if (touch_info & 0x01)
			CPRINTF("click|");

		for (i = 0; i < ETP_MAX_FINGERS; i++) {
			int valid = touch_info & (1 << (3+i));

			if (valid) {
				int x = ((finger[0] & 0xf0) << 4) | finger[1];
				int y = ((finger[0] & 0x0f) << 8) | finger[2];
				int width = (finger[3] & 0xf0) >> 4;
				int height = finger[3] & 0x0f;
				int pressure = finger[4];

				if (1)
					CPRINTF("i=%d %d/%d %d/%d %d|", i, x, y,
						width, height, pressure);
				finger += ETP_FINGER_DATA_LEN;
			}
		}
		CPRINTF("]\n");
	}

	return rv;
}

/* Initialize the controller ICs after reset */
static void elan_tp_init(void)
{
	int rv;

	CPRINTF("[%T ELAN_TP_init]\n");

	elan_tp_write_cmd(ETP_I2C_STAND_CMD, ETP_I2C_RESET);
	msleep(100);
	*((uint16_t *)tp_buf) = 0xDEAD;
	rv = i2c_xfer(I2C_PORT_TOUCHPAD, I2C_ADDR_TOUCHPAD, NULL, 0, tp_buf,
		      ETP_I2C_INF_LENGTH, I2C_XFER_SINGLE);
	CPRINTF("[%T rv %d buf=%04x]\n", rv, *((uint16_t *)tp_buf));

	/* FIXME: What's the purpose of these? */
	rv = elan_tp_read_block(ETP_I2C_DESC_CMD, tp_buf, ETP_I2C_DESC_LENGTH);
	rv = elan_tp_read_block(ETP_I2C_REPORT_DESC_CMD, tp_buf,
				ETP_I2C_REPORT_DESC_LENGTH);

	/* Switch to absolute mode */
	rv = elan_tp_write_cmd(ETP_I2C_SET_CMD, ETP_ENABLE_ABS);

	/* Sleep control off */
	rv = elan_tp_write_cmd(ETP_I2C_STAND_CMD, ETP_I2C_WAKE_UP);
}

void elan_tp_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_TOUCHPAD);
}

void elan_tp_task(void)
{
	elan_tp_init();

	gpio_enable_interrupt(GPIO_TOUCHPAD_INT);

	while (1) {
		elan_tp_read_report();

		task_wait_event(-1);
	}
}
