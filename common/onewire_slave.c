/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* 1-wire interface module for Chrome EC (slave) */

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

	/* Wait for low */
	while (!gpio_get_level(GPIO_ONEWIRE)) {}

	udelay(T_MSR);

	/* Read bit */
	bit = !gpio_get_level(GPIO_ONEWIRE);

	/*
	 * Enable interrupt as soon as we've read the bit.  The delay to the
	 * end of the timeslot is a lower bound, so additional latency here is
	 * harmless.
	 */
	interrupt_enable();

	/* Wait for high */
	while (gpio_get_level(GPIO_ONEWIRE)) {}

	return bit;
}

/**
 * Write a bit.
 */
static void writebit(int bit)
{

	/*
	 * The delay between sending the output pulse and reading the bit is
	 * extremely timing sensitive, so disable interrupts.
	 */
	interrupt_disable();

	/* Wait for low */
	while (!gpio_get_level(GPIO_ONEWIRE)) {}

	if (!bit) {
		output0(T_MSR*2);
		udelay(T_MSR*2);
	}

	interrupt_enable();

	/* Wait for high */
	while (gpio_get_level(GPIO_ONEWIRE)) {}
}

static int onewire_slave_read(void)
{
	int data = 0;
	int i;

	for (i = 0; i < 8; i++)
		data |= readbit() << i;  /* LSB first */

	return data;
}

static void onewire_slave_write(int data)
{
	int i;

	for (i = 0; i < 8; i++)
		writebit((data >> i) & 0x01);  /* LSB first */
}

static void onewire_slave_handler(void);
DECLARE_DEFERRED(onewire_slave_handler);

static void onewire_slave_handler(void)
{
	int data;

	gpio_disable_interrupt(GPIO_ONEWIRE);

	interrupt_disable();
	/* Wait for end of reset pulse */
	while (gpio_get_level(GPIO_ONEWIRE)) {}

	output0(T_MSP);
	interrupt_enable();
	udelay(T_MSP/2);

	ccprintf("R");
	data = onewire_slave_read();
	ccprintf("W");
	onewire_slave_write(~data & 0xff);

	ccprintf("Read %02x\n", data);
	gpio_enable_interrupt(GPIO_ONEWIRE);

	usleep(100000);

	hook_call_deferred(&onewire_slave_handler_data, -1);
}

void onewire_slave_interrupt(enum gpio_signal signal)
{
	ccprintf("C%d", gpio_get_level(GPIO_ONEWIRE));

	hook_call_deferred(&onewire_slave_handler_data, 0);
}

/* slow functions [4.108283 high=4210, low=4292] */
/* fast functions  */

static int command_udelay_test(int argc, char **argv)
{
	int i;
	const int loop = 1000;
	timestamp_t t1, t2, t3;

	gpio_disable_interrupt(GPIO_ONEWIRE);
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
	gpio_enable_interrupt(GPIO_ONEWIRE);

	ccprints("high=%d, low=%d", t3.le.lo-t2.le.lo, t2.le.lo-t1.le.lo);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(udelay, command_udelay_test,
		NULL, "udelay");
