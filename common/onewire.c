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
#include "task.h"
#include "timer.h"

/*
 * Standard speed; all timings padded by 2 usec for safety.
 *
 * Note that these timing are actually _longer_ than legacy 1-wire standard
 * speed because we're running the 1-wire bus at 3.3V instead of 5V.
 */
#define T_RSTL 602  /* Reset low pulse; 600-960 us */
#define T_MSP   72  /* Presence detect sample time; 70-75 us */
#define T_RSTH (68 + 260 + 5 + 2) /* Reset high; tPDHmax + tPDLmax + tRECmin */
#define T_SLOT  70  /* Timeslot; >67 us */
#define T_W0L   63  /* Write 0 low; 62-120 us */
#define T_W1L    7  /* Write 1 low; 5-15 us */
#define T_RL     7  /* Read low; 5-15 us */
#define T_MSR   15  /* Read sample time; <15 us.  Must be at least 200 ns after
		     * T_RL since that's how long the signal takes to be pulled
		     * up on our board.  */

/**
 * Output low on the bus for <usec> us, then switch back to open-drain input.
 */
static void output0(int usec)
{
	gpio_set_flags(GPIO_ONEWIRE,
		       GPIO_OUTPUT | GPIO_OUT_HIGH);
	udelay(usec);
	gpio_set_flags(GPIO_ONEWIRE, GPIO_INPUT);
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
 * Read a bit.
 */
static int readbit_slave(void)
{
	int bit;

	/*
	 * The delay between sending the output pulse and reading the bit is
	 * extremely timing sensitive, so disable interrupts.
	 */
	interrupt_disable();

	/* Wait for low */
	while (!gpio_get_level(GPIO_ONEWIRE)) {}

	udelay(30);

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

/**
 * Write a bit.
 */
static void writebit_slave(int bit)
{

	/*
	 * The delay between sending the output pulse and reading the bit is
	 * extremely timing sensitive, so disable interrupts.
	 */
	interrupt_disable();

	/* Wait for low */
	while (!gpio_get_level(GPIO_ONEWIRE)) {}

	if (!bit) {
		output0(30);
		udelay(10);
	} else {
		udelay(30);
	}

	interrupt_enable();
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

int onewire_read_slave(void)
{
	int data = 0;
	int i;

	for (i = 0; i < 8; i++)
		data |= readbit_slave() << i;  /* LSB first */

	return data;
}

void onewire_write(int data)
{
	int i;

	for (i = 0; i < 8; i++)
		writebit((data >> i) & 0x01);  /* LSB first */
}

void onewire_write_slave(int data)
{
	int i;

	for (i = 0; i < 8; i++)
		writebit_slave((data >> i) & 0x01);  /* LSB first */
}

static void onewire_handler(void);
DECLARE_DEFERRED(onewire_handler);

/* Set PD discharge whenever VBUS detection is high (i.e. below threshold). */
static void onewire_handler(void)
{
	int data;

	gpio_disable_interrupt(GPIO_ONEWIRE);

	interrupt_disable();
	/* Wait for end of reset pulse */
	while (gpio_get_level(GPIO_ONEWIRE)) {}

	output0(T_MSP*2);
	interrupt_enable();
	udelay(T_MSP/2);

	ccprintf("R");
	data = onewire_read_slave();
	ccprintf("W");
	onewire_write_slave(~data & 0xff);

	ccprintf("Read %02x\n", data);

	gpio_enable_interrupt(GPIO_ONEWIRE);

	usleep(100000);

	hook_call_deferred(&onewire_handler_data, -1);
}

void onewire_interrupt(enum gpio_signal signal)
{
	ccprintf("C%d", gpio_get_level(GPIO_ONEWIRE));

	hook_call_deferred(&onewire_handler_data, 0);
}

static int command_onewire_send(int argc, char **argv)
{
	static int cnt = 0x96;
	int rv;
	int data;
	gpio_disable_interrupt(GPIO_ONEWIRE);
	rv = onewire_reset();
	if (rv)
		return rv;

	onewire_write(cnt);

	data = onewire_read();
	ccprintf("wrote %02x, read %02x (%02x)\n", cnt, data, ~data & 0xff);

	gpio_enable_interrupt(GPIO_ONEWIRE);
	
	cnt++;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(onewire, command_onewire_send,
		NULL, "Send onewire data");

/*
loop = 1
[11.187631 udelay test start]
[11.287998 udelay test stop]

loop = 1000 (sleep 100 us)
[5.484641 udelay test start]
[5.589714 udelay test stop]

loop = 10000 (sleep 10 us)
[5.398594 udelay test start]
[5.529256 udelay test stop]

loop = 20000 (sleep 5 us)
[3.152793 udelay test start 20000]
[3.337790 udelay test stop]

[23.094405 udelay test start 1000]
[23.320815 udelay test stop 0]
226 us per loop, so gpio_set_flags takes about 126 us!!
*/

static int command_udelay_test(int argc, char **argv)
{
	int i;
	int k = 0;
	const int loop = 1000;

	ccprints("udelay test start %d", loop);

	interrupt_disable();

	for (i = 0; i < loop; i++) {
/*		k += gpio_get_level(GPIO_ONEWIRE);
		udelay(100000/loop);*/
		gpio_set_flags(GPIO_ONEWIRE,
		GPIO_OUTPUT | GPIO_OUT_HIGH);
		udelay(100000/loop);
		gpio_set_flags(GPIO_ONEWIRE, GPIO_INPUT);
	}

	interrupt_enable();
	
	ccprints("udelay test stop %d", k);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(udelay, command_udelay_test,
		NULL, "udelay");
