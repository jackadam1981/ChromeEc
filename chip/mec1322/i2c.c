/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C port module for MEC1322 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "timer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

/* Status */
#define STS_nBB (1 << 0) /* Bus busy */
#define STS_LAB (1 << 1) /* Arbitration lost */
#define STS_LRB (1 << 3) /* Last received bit */
#define STS_BER (1 << 4) /* Bus error */
#define STS_PIN (1 << 7) /* Pending interrupt */

/* Control */
#define CTRL_ACK (1 << 0) /* Acknowledge */
#define CTRL_STO (1 << 1) /* STOP */
#define CTRL_STA (1 << 2) /* START */
#define CTRL_ENI (1 << 3) /* Enable interrupt */
#define CTRL_ESO (1 << 6) /* Enable serial output */
#define CTRL_PIN (1 << 7) /* Pending interrupt not */

static void configure_port(int port, int kbps)
{
	MEC1322_I2C_CTRL(port) = CTRL_PIN;
	MEC1322_I2C_OWN_ADDR(port) = 0x0;
	if (kbps == 400)
		MEC1322_I2C_BUS_CLK(port) = 0x0f17;
	else if (kbps == 1000)
		MEC1322_I2C_BUS_CLK(port) = 0x0509;
	else /* Assume 100kbps */
		MEC1322_I2C_BUS_CLK(port) = 0x4f4f;
	MEC1322_I2C_CTRL(port) = CTRL_PIN | CTRL_ESO | CTRL_ACK;
	MEC1322_I2C_CONFIG(port) |= 1 << 10; /* ENAB */
}

static void reset_port(int port)
{
	int i;

	MEC1322_I2C_CONFIG(port) |= 1 << 9;
	udelay(100);
	MEC1322_I2C_CONFIG(port) &= ~(1 << 9);

	for (i = 0; i < i2c_ports_used; ++i)
		if (port == i2c_ports[i].port) {
			configure_port(i2c_ports[i].port, i2c_ports[i].kbps);
			break;
		}
}

static int wait_idle(int port)
{
	/* TODO: handle timeout. use task_wait */
	while (!(MEC1322_I2C_STATUS(port) & STS_nBB))
		;
	return EC_SUCCESS;
}

int i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_size,
	     uint8_t *in, int in_size, int flags)
{
	int i;
	int started = (flags & I2C_XFER_START) ? 0 : 1;
	uint32_t reg_sts;

	if (out_size == 0 && in_size == 0)
		return EC_SUCCESS;

	reg_sts = MEC1322_I2C_STATUS(port);
	if (!started && (reg_sts & (STS_BER | STS_LAB))) {
		CPRINTF("[%T I2C%d bad status 0x%02x]\n", port, reg_sts);

		/* Bus error or arbitration lost. Reset port. */
		reset_port(port);

		/*
		 * We don't know what edges the slave saw, so sleep long enough
		 * that the slave will see the new start condition below.
		 */
		usleep(1000);
	}

	if (out) {
		wait_idle(port);
		MEC1322_I2C_DATA(port) = slave_addr & 0xff;
		MEC1322_I2C_CTRL(port) = CTRL_PIN | CTRL_ESO |
					 CTRL_ACK | (started ? 0 : CTRL_STA);
		if (!started)
			started = 1;
		for (i = 0; i < out_size; ++i) {
			while (MEC1322_I2C_STATUS(port) & STS_PIN)
				;
			/* TODO: handle NACK in STATUS/LRB */
			MEC1322_I2C_DATA(port) = out[i];
		}
		/*
		 * Send stop bit if the stop flag is on, and caller
		 * doesn't expect to receive data.
		 */
		if ((flags & I2C_XFER_STOP) && in_size == 0)
			MEC1322_I2C_CTRL(port) = CTRL_PIN | CTRL_ESO |
						 CTRL_STO | CTRL_ACK;
	}

	if (in_size) {
		if (out_size) {
			/* resend start bit when change direction */
			MEC1322_I2C_CTRL(port) = CTRL_ESO | CTRL_STA | CTRL_ACK;
		}

		MEC1322_I2C_DATA(port) = (slave_addr & 0xff) | 0x01;

		wait_idle(port);

		if (!started) {
			started = 1;
			MEC1322_I2C_CTRL(port) = CTRL_PIN | CTRL_ESO |
						 CTRL_STA | CTRL_ACK;
		}
		for (i = 0; i < in_size - 2; ++i) {
			while (MEC1322_I2C_STATUS(port) & STS_PIN)
				;
			/* TODO: handle NACK in STATUS/LRB */
			in[i] = MEC1322_I2C_DATA(port);
		}
		MEC1322_I2C_CTRL(port) = CTRL_ESO;
		in[in_size - 2] = MEC1322_I2C_DATA(port);
		while (MEC1322_I2C_STATUS(port) & STS_PIN)
			;
		MEC1322_I2C_CTRL(port) =
			CTRL_PIN | CTRL_ESO | CTRL_ACK | 
			((flags & I2C_XFER_STOP) ? CTRL_STO : 0);
		in[in_size - 1] = MEC1322_I2C_DATA(port);
	}

	if (MEC1322_I2C_STATUS(port) & (STS_LAB | STS_BER))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

int i2c_get_line_levels(int port)
{
	/* TODO: implement */
	return I2C_LINE_IDLE;
}

static void i2c_init(void)
{
	int i;

	/* Configure GPIOs */
	gpio_config_module(MODULE_I2C, 1);

	for (i = 0; i < i2c_ports_used; ++i)
		configure_port(i2c_ports[i].port, i2c_ports[i].kbps);
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_DEFAULT);

static int cmd_test_i2c(int argc, char **argv)
{
	while (!(MEC1322_I2C_STATUS(0) & 0x1)) /* nBB */
		;
	MEC1322_I2C_DATA(0) = 0x62; /* Slave addr + W */
	MEC1322_I2C_CTRL(0) = 0xc5;
	while (MEC1322_I2C_STATUS(0) & 0x80) /* PIN */
		;
	MEC1322_I2C_CTRL(0) = 0xc3;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(testi2c, cmd_test_i2c, NULL, NULL, NULL);
