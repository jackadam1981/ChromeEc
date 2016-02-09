/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* I2C port module for Rotor MCU */

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

/* Supported i2c input clocks */
#define I2C_CLK_SRC_25MHZ 25000000
#define I2C_CLK_SRC_8MHZ  8000000 /* TODO(aaboagye): This is the ANA_GRP DRO,
				   * but I'm not sure of the exact frequency.
				   * Clock diagram shows a range from 8-24MHz
				   */
#define I2C_CLK_SRC_32KHZ 32000

/* Task waiting on port, or TASK_ID_INVALID if none. */
static volatile int task_waiting[I2C_PORT_COUNT] = { TASK_ID_INVALID };

/**
 * Abort the current transaction.
 *
 * The controller will send a STOP condition and flush the TX FIFO.
 *
 * @param port	The port where you wish to abort the transaction.
 */
static void abort_transfer(int port)
{
	/* Unmask the M_TX_ABRT interrupt. */
	ROTOR_MCU_I2C_INTR_MASK(port) = (1 << 6);
	task_enable_irq(ROTOR_MCU_IRQ_I2C_0 + port);

	/* Issue the abort. */
	ROTOR_MCU_I2C_ENABLE(port) |= (1 << 1);
	CPRINTS("i2c xfer abort.");

	/* Wait for the interrupt to fire. */
	task_wait_event_mask(TASK_EVENT_I2C_IDLE, 500);

	/* Mask the M_TX_ABRT interrupt. */
	ROTOR_MCU_I2C_INTR_MASK(port) = 0;
	task_disable_irq(ROTOR_MCU_IRQ_I2C_0 + port);
}

/**
 * Disable the I2C port.
 *
 * @param port	The port which you wish to disable.
 * @return EC_SUCCESS on successfully disabling the port, non-zero otherwise.
 */
static int disable_i2c(int port)
{
	uint8_t timeout = 50;

	/* Check if the hardware is already shutdown. */
	if (!(ROTOR_MCU_I2C_ENABLE_STATUS(port) & (1 << 0)))
		return EC_SUCCESS;

	/* Try disabling the port. */
	ROTOR_MCU_I2C_ENABLE(port) &= ~(1 << 0);

	/* Check to see that the hardware actually shuts down. */
	while (ROTOR_MCU_I2C_ENABLE_STATUS(port) & (1 << 0)) {
		usleep(10);
		timeout--;

		if (timeout == 0)
			return EC_ERROR_TIMEOUT;
	};

	return EC_SUCCESS;
}

/**
 * Wait until the byte has been popped from the TX FIFO.
 *
 * This interrupt is automatically cleared by hardware when the buffer level
 * goes above the threshold (set to one element).
 *
 * @param port		The i2c port to wait for.
 * @param timeout	The timeout in microseconds.
 */
static int wait_byte_done(int port, int timeout)
{
	uint32_t events;

	if (timeout <= 0 )
		return EC_ERROR_TIMEOUT;

	/* Unmask the TX_EMPTY interrupt. */
	ROTOR_MCU_I2C_INTR_MASK(port) |= (1 << 4);
	task_waiting[port] = task_get_current();
	task_enable_irq(ROTOR_MCU_IRQ_I2C_0 + port);

	/* Wait until the interrupt fires. */
	events = task_wait_event_mask(TASK_EVENT_I2C_IDLE, timeout);

	task_waiting[port] = TASK_ID_INVALID;
	task_disable_irq(ROTOR_MCU_IRQ_I2C_0 + port);
	/* Mask the TX_EMPTY interrupt. */
	ROTOR_MCU_I2C_INTR_MASK(port) &= ~(1 << 4);

	return (events & TASK_EVENT_TIMER) ? EC_ERROR_TIMEOUT : EC_SUCCESS;
}

/**
 * Wait until the byte has been inserted to the RX FIFO.
 *
 * Since the RX transmission level is set to only 1 element, the RX_FULL
 * interrupt should fire when there's at least 1 new byte to read.  This
 * interrupt is automatically cleared by hardware when the buffer level goes
 * below the threshold (set to one element).
 *
 * @param port		The i2c port to wait for.
 * @param timeout	The timeout in microseconds.
 */
static int wait_byte_ready(int port, int timeout)
{
	uint32_t events;

	if (timeout <= 0)
		return EC_ERROR_TIMEOUT;

	/* Unmask the RX_FULL interrupt. */
	ROTOR_MCU_I2C_INTR_MASK(port) |= (1 << 2);
	task_waiting[port] = task_get_current();
	task_enable_irq(ROTOR_MCU_IRQ_I2C_0 + port);

	/* Wait until the interrupt fires. */
	events = task_wait_event_mask(TASK_EVENT_I2C_IDLE, timeout);

	task_disable_irq(ROTOR_MCU_IRQ_I2C_0 + port);
	task_waiting[port] = TASK_ID_INVALID;
	/* Mask the RX_FULL interrupt. */
	ROTOR_MCU_I2C_INTR_MASK(port) &= ~(1 << 2);

	return (events & TASK_EVENT_TIMER) ? EC_ERROR_TIMEOUT : EC_SUCCESS;
}

int chip_i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_size,
		  uint8_t *in, int in_size, int flags)
{
	int i;
	int rv;
	uint64_t task_timeout = get_time().val + I2C_TIMEOUT_DEFAULT_US;;
	/* Check if there's anything we actually have to do. */
	if (!in_size && !out_size)
		return EC_SUCCESS;

	/* Make sure we're in a good state to start. */
	if ((flags & I2C_XFER_START) &&
	    i2c_get_line_levels(port) != I2C_LINE_IDLE) {
		CPRINTS("I2C%d Addr:%02X bad status SCL=%d, SDA=%d",
			port,
			slave_addr,
			i2c_get_line_levels(port) & I2C_LINE_SCL_HIGH,
			i2c_get_line_levels(port) & I2C_LINE_SDA_HIGH ? 1 : 0);

		/* Attempt to unwedge the port. */
		i2c_unwedge(port);
	}

	/*
	 * This i2c block doesn't let us manually control when to set the START
	 * condition or the STOP condition.  So, we'll have to ignore that flag
	 * for now.  TODO(aaboagye): Is there a way around this?
	 */

	/* Set the slave address. */
	ROTOR_MCU_I2C_TAR(port) = (slave_addr >> 1) & 0xFF;
	if (out_size) {
		/*
		 * Placing data into the TX FIFO causes the i2c block to
		 * generate a START condition on the bus.
		 *
		 * A STOP condition will be generated when the TX FIFO is empty.
		 */
		for (i=0; i<out_size; i++) {
			ROTOR_MCU_I2C_DATA_CMD(port) = out[i];
			/* Wait until byte popped from TX FIFO. */
			rv = wait_byte_done(port,
					    task_timeout - get_time().val);
			if (rv != EC_SUCCESS) {
				/* Abort the transaction. */
				abort_transfer(port);
				return EC_ERROR_TIMEOUT;
			}
		}
	}

	/* The i2c block generates a RESTART when the direction changes. */
	if (in_size) {
		for (i=0; i<in_size; i++) {
			/*
			 * In order for the i2c block to continue acknowledging
			 * reads, a read command must be written for every byte
			 * that is to be received.
			 */
			ROTOR_MCU_I2C_DATA_CMD(port) = (1 << 8);
			/* Wait for RX_FULL interrupt. */
			rv = wait_byte_ready(port,
					     task_timeout - get_time().val);
			if (rv != EC_SUCCESS) {
				/* Abort the transaction. */
				abort_transfer(port);
				return EC_ERROR_TIMEOUT;
			}
			/* Retrieve the byte from the RX FIFO. */
			in[i] = (ROTOR_MCU_I2C_DATA_CMD(port) & 0xFF);
		}
	}

	task_waiting[port] = TASK_ID_INVALID;
	return EC_SUCCESS;
}

/**
 * Set up the port with the requested speeds.
 *
 * @param port	I2C port being configured.
 * @param freq	The desired operation speed of the port.
 */
static void set_port_speed(int port, enum i2c_freq freq)
{
	int i2c_clk;
	int period;
	int t_low;
	int t_high;

	/* Try and determine the current i2c clock source .*/
	switch ((ROTOR_MCU_I2C_REFCLKGEN(port) & (0x3 << 24)) >> 24)
	{
	case 0: /* ANA_GRP XTAL */
		i2c_clk = I2C_CLK_SRC_25MHZ;
		break;
	case 1: /* EXT 32KHz CLK */
		i2c_clk = I2C_CLK_SRC_32KHZ;
		break;
	case 2: /* ANA_GRP DRO CLK */
		i2c_clk = I2C_CLK_SRC_8MHZ;
		break;
	case 3: /* APLL0 CLK */
		/* Something like 589MHz?? */
		break;
	default:
		break;
	};

	/* Set the count registers for the approriate timing. */
	ROTOR_MCU_I2C_CON(port) &= (0x3 << 1);
	switch (freq)
	{
	case I2C_FREQ_100KHZ:
		ROTOR_MCU_I2C_CON(port) |= (1 << 1);
		/*
		 * Standard mode Minimum times according to spec:
		 *   t_high = 4.0 us
		 *   t_low = 4.7 us
		 */
		period = i2c_clk / 1000 / 100;
		t_high = MAX((i2c_clk * 4000 / 1000000000), period / 2);
		t_low = MAX((i2c_clk * 4700 / 1000000000), period / 2);
		ROTOR_MCU_I2C_SS_SCL_HCNT(port) |= (t_high & 0xFFFF);
		ROTOR_MCU_I2C_SS_SCL_LCNT(port) |= (t_low & 0xFFFF);
		break;
	case I2C_FREQ_400KHZ: /* Fast Mode */
		ROTOR_MCU_I2C_CON(port) |= (2 << 1);
		/*
		 * Fast mode minimum times
		 *   t_high = 0.6 us
		 *   t_low = 1.3 us
		 */
		period = i2c_clk / 1000 / 400;
		t_high = MAX((i2c_clk * 600 / 1000000000), period / 2);
		t_low = MAX((i2c_clk * 1300 / 1000000000), period / 2);
		ROTOR_MCU_I2C_FS_SCL_HCNT(port) |= (t_high & 0xFFFF);
		ROTOR_MCU_I2C_FS_SCL_LCNT(port) |= (t_low & 0xFFFF);
		break;
	case I2C_FREQ_1000KHZ: /* Fast Mode Plus */
		ROTOR_MCU_I2C_CON(port) |= (3 << 1);
		/*
		 * Fast mode plus minimum times
		 *   t_high = 0.26 us
		 *   t_low = 0.5 us
		 */
		period = i2c_clk / 1000 / 1000;
		t_high = MAX((i2c_clk * 260 / 1000000000), period / 2);
		t_low = MAX((i2c_clk * 500 / 1000000000), period / 2);
		ROTOR_MCU_I2C_HS_SCL_HCNT(port) |= (t_high & 0xFFFF);
		ROTOR_MCU_I2C_HS_SCL_LCNT(port) |= (t_low & 0xFFFF);
		break;
	default:
		break;
	};
}

/**
 * Initialize the specified I2C port.
 *
 * @param p	the I2C port.
 */
static void i2c_init_port(const struct i2c_port_t *p)
{
	int port = p->port;
	enum i2c_freq freq;

	/* Enable the clock for the port if necessary. */
	if (!(ROTOR_MCU_I2C_REFCLKGEN(port) & (1 << 1)))
		ROTOR_MCU_I2C_REFCLKGEN(port) |= (1 << 1);

	/* Disable the I2C block to allow changes to certain regsiters. */
	disable_i2c(port);

	/*
	 * Mask all interrupts right now.  We'll unmask the ones we need as we
	 * go.
	 */
	ROTOR_MCU_I2C_INTR_MASK(port) = 0;

	/*
	 * Configure as I2C master allowing RESTART conditions and using 7-bit
	 * addressing.
	 */
	ROTOR_MCU_I2C_CON(port) = (1 << 6) | (1 << 5) | (1 << 0);

	/* Set operation speed. */
	switch (p->kbps) {
	case 1000: /* Fast-mode Plus */
		freq = I2C_FREQ_1000KHZ;
		break;
	case 400: /* Fast-mode */
		freq = I2C_FREQ_400KHZ;
		break;
	case 100: /* Standard-mode */
		freq = I2C_FREQ_100KHZ;
		break;
	default: /* unknown speed, defaults to 100kBps */
		CPRINTS("I2C bad speed %d kBps", p->kbps);
		freq = I2C_FREQ_100KHZ;
	}
	/* TODO(aaboagye): Verify that the frequency is set correctly. */
	set_port_speed(port, freq);

	/* Enable the port. */
	ROTOR_MCU_I2C_ENABLE(port) |= (1 << 0);
}

/**
 * Initialize the i2c module for all supported ports.
 */
static void i2c_init(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	for (i = 0; i < i2c_ports_used; i++, p++)
		i2c_init_port(p);

	/* Configure the GPIO pins for I2C usage. */
	gpio_config_module(MODULE_I2C, 1);
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_INIT_I2C);

int i2c_get_line_levels(int port)
{
	return (i2c_raw_get_sda(port) ? I2C_LINE_SDA_HIGH : 0) |
		(i2c_raw_get_scl(port) ? I2C_LINE_SCL_HIGH : 0);
}

int i2c_raw_get_scl(int port)
{
	enum gpio_signal pin;

	if (get_scl_from_i2c_port(port, &pin) == EC_SUCCESS)
		return gpio_get_level(pin);

	/* If no SCL pin defined for this port, then return 1 to appear idle. */
	return 1;
}

int i2c_raw_get_sda(int port)
{
	enum gpio_signal pin;

	if (get_sda_from_i2c_port(port, &pin) == EC_SUCCESS)
		return gpio_get_level(pin);

	/* If no SDA pin defined for this port, then return 1 to appear idle. */
	return 1;
}

/**
 * Handle an interrupt on the specified port.
 *
 * @param port	I2C port generating interrupt
 */
static void handle_interrupt(int port)
{
	int id = task_waiting[port];

	/* Clear software clearable interrupt status. */
	if (ROTOR_MCU_I2C_CLR_INTR(port))
		;

	/* If no task is waiting, just return. */
	if (id == TASK_ID_INVALID)
		return;

	/* Wake up the task which was waiting for the interrupt. */
	task_set_event(id, TASK_EVENT_I2C_IDLE, 0);
}

void i2c0_interrupt(void) { handle_interrupt(0); }
void i2c1_interrupt(void) { handle_interrupt(1); }
void i2c2_interrupt(void) { handle_interrupt(2); }
void i2c3_interrupt(void) { handle_interrupt(3); }
void i2c4_interrupt(void) { handle_interrupt(4); }
void i2c5_interrupt(void) { handle_interrupt(5); }

DECLARE_IRQ(ROTOR_MCU_IRQ_I2C_0, i2c0_interrupt, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_I2C_1, i2c1_interrupt, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_I2C_2, i2c2_interrupt, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_I2C_3, i2c3_interrupt, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_I2C_4, i2c4_interrupt, 2);
DECLARE_IRQ(ROTOR_MCU_IRQ_I2C_5, i2c5_interrupt, 2);
