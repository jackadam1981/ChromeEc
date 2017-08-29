/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* 1-wire interface module for Chrome EC */

#include <stddef.h>
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "onewire_common.h"
#include "onewire.h"
#include "task.h"
#include "timer.h"

/**
 * Output low on the bus for <usec> us, then switch back to open-drain input.
 */
static void output0(int usec)
{
	gpio_set_level_tristate(GPIO_ONEWIRE, 1);
	udelay(usec);
	gpio_set_level_tristate(GPIO_ONEWIRE, 2);
}

/**
 * Read a bit.
 */
static int readbit(void)
{
	int bit;

	/*
	 * The delay between sending the output pulse and reading the bit is
	 * extremely timing sensitive, so disable interrupts.
	 */
	interrupt_disable();

	/* Output low */
	output0(T_RL);

	/* Delay to let slave release the line if it wants to send a 1-bit */
	udelay(T_MSR - T_RL);

	/* Read bit */
	bit = !gpio_get_level(GPIO_ONEWIRE);

	/*
	 * Enable interrupt as soon as we've read the bit.  The delay to the
	 * end of the timeslot is a lower bound, so additional latency here is
	 * harmless.
	 */
	interrupt_enable();

	/* Delay to end of timeslot */
	udelay(T_SLOT - T_MSR);
	return bit;
}

/**
 * Write a bit.
 */
static void writebit(int bit)
{
	/*
	 * The delays in the output-low signal for sending 0 and 1 bits are
	 * extremely timing sensitive, so disable interrupts during that time.
	 * Interrupts can be enabled again as soon as the output is switched
	 * back to open-drain, since the delay for the rest of the timeslot is
	 * a lower bound.
	 */
	if (bit) {
		interrupt_disable();
		output0(T_W1L);
		interrupt_enable();
		udelay(T_SLOT - T_W1L);
	} else {
		interrupt_disable();
		output0(T_W0L);
		interrupt_enable();
		udelay(T_SLOT - T_W0L);
	}

}

int onewire_reset(void)
{
	/* Start transaction with master reset pulse */
	output0(T_RSTL);

	/* Wait for presence detect sample time.
	 *
	 * (Alternately, we could poll waiting for a 1-bit indicating our pulse
	 * has let go, then poll up to max time waiting for a 0-bit indicating
	 * the slave has responded.)
	 */
	udelay(T_MSP);

	if (!gpio_get_level(GPIO_ONEWIRE))
		return EC_ERROR_UNKNOWN;

	/*
	 * Wait for end of presence pulse.
	 *
	 * (Alternately, we could poll waiting for a 1-bit.)
	 */
	udelay(T_RSTH - T_MSP);

	return EC_SUCCESS;
}

int onewire_read(void)
{
	int data = 0;
	int i;

	for (i = 0; i < 8; i++)
		data |= readbit() << i;  /* LSB first */

	return data;
}

void onewire_write(int data)
{
	int i;

	for (i = 0; i < 8; i++)
		writebit((data >> i) & 0x01);  /* LSB first */
}

static void onewire_send_loop(void);
DECLARE_DEFERRED(onewire_send_loop);

static void onewire_send_loop(void)
{
	static int cnt = 0;
	static int errreset = 0;
	static int errdata = 0;
	int rv;
	int data;
	gpio_disable_interrupt(GPIO_ONEWIRE);
	rv = onewire_reset();
	if (rv) {
		errreset++;
		goto out;
	}

	onewire_write(cnt & 0xff);

	data = onewire_read();
	ccprintf("wrote %02x, read %02x (%02x)\n",
		cnt & 0xff, data, ~data & 0xff);

	gpio_enable_interrupt(GPIO_ONEWIRE);

	if ((cnt & 0xff) != (~data & 0xff)) {
		errdata++;
	}

out:
	cnt++;
	ccprintf("1wire errors %d-%d/%d\n", errreset, errdata, cnt);
	hook_call_deferred(&onewire_send_loop_data, 500*MSEC);
}

static int command_onewire_send(int argc, char **argv)
{
	hook_call_deferred(&onewire_send_loop_data, 500*MSEC);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(onewire, command_onewire_send,
		NULL, "Send onewire data");

/* slow functions [9.995142 high=71204, low=46948] */
/* fast functions [7.665873 high=1337, low=2670] */

static int command_udelay_test(int argc, char **argv)
{
	int i;
	const int loop = 1000;
	timestamp_t t1, t2, t3;

	interrupt_disable();
	t1 = get_time();
	for (i = 0; i < loop; i++) {
		gpio_set_level_tristate(GPIO_ONEWIRE, 1);
	}
	t2 = get_time();
	for (i = 0; i < loop; i++) {
		gpio_set_level_tristate(GPIO_ONEWIRE, 2);
	}
	t3 = get_time();
	interrupt_enable();

	ccprints("high=%d, low=%d", t3.le.lo-t2.le.lo, t2.le.lo-t1.le.lo);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(udelay, command_udelay_test,
		NULL, "udelay");
