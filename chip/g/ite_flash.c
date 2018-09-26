/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ccd_config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "hooks.h"
#include "i2c.h"
#include "init_chip.h"
#include "ite_sync.h"
#include "registers.h"
#include "scratch_reg1.h"
#include "system.h"
#include "timer.h"
#include "usb_i2c.h"

#define ITE_SYNC_TIME  (50 * MSEC)
#define ITE_PERIOD_TIME 5  /* This is 200 kHz */
#define TIMEUS_CLK_FREQ 24 /* units: MHz */
#define HALF_PERIOD_TICKS 8

static void wreg(uint32_t addr, uint32_t data)
{
	*((volatile uint32_t *)addr) = data;
}

static void suppress_jitter(void)
{
	init_jittery_clock_locking_optional(1, 0, 0);
	wreg(0x4009A6D0, 0);
}

/*
 * Callback invoked by usb_i2c bridge when a wite to a special I2C address is
 * requested. We don't really care about any data in this case, when invoked
 * disable jitter, generate sync sequence and enable jitter back.
 */
void maybe_trigger_ite_sync(void)
{
	volatile uint16_t *gpio_addr;
	uint32_t cycle_count;
	uint16_t both_zero;
	uint16_t both_one;
	uint16_t one_zero;
	uint16_t zero_one;
	uint32_t lls1;

	lls1 = GREG32(PMU, LONG_LIFE_SCRATCH1);

	if (!(lls1 & BOARD_ITE_EC_SYNC_NEEDED))
		return;

	/* Clear the sync required bit. */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 1);
	GREG32(PMU, LONG_LIFE_SCRATCH1) = lls1 & ~BOARD_ITE_EC_SYNC_NEEDED;
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 0);

	/* Let's pulse the EC while preparing to sync up. */
	assert_ec_rst();
	msleep(1);
	deassert_ec_rst();
	msleep(5);

	/*
	 * Values to write to set SCL and SDA to various combinations of 0 and
	 * 1 to be able to generate two necessary waveforms.
	 */
	both_zero = 0;
	one_zero = 1 << 13;
	zero_one = 1 << 12;
	both_one = one_zero | zero_one;

	/* Address of the mask byte register to use to set both pins. */
	gpio_addr = (uint16_t *) (GC_GPIO0_BASE_ADDR +
				  GC_GPIO_MASKHIGHBYTE_800_OFFSET +
				  (both_one >> 8) * 4);

	REG32(GBASE(PINMUX) + GOFFSET(PINMUX, DIOB0_SEL)) =
		GC_PINMUX_GPIO0_GPIO12_SEL;
	REG32(GBASE(PINMUX) + GOFFSET(PINMUX, DIOB1_SEL)) =
		GC_PINMUX_GPIO0_GPIO13_SEL;

	gpio_set_flags(GPIO_I2C_SCL_INA, GPIO_OUTPUT | GPIO_HIGH);
	gpio_set_flags(GPIO_I2C_SDA_INA, GPIO_OUTPUT | GPIO_HIGH);

	cycle_count = 2 * ITE_SYNC_TIME / ITE_PERIOD_TIME;

	interrupt_disable();
	suppress_jitter();

	ite_sync(gpio_addr, both_zero, one_zero, zero_one, both_one,
		 HALF_PERIOD_TICKS, HALF_PERIOD_TICKS * cycle_count);

	interrupt_enable();

	/* Restore I2C configuration. */
	gpio_set_flags(GPIO_I2C_SCL_INA, GPIO_PULL_UP);
	gpio_set_flags(GPIO_I2C_SDA_INA, GPIO_PULL_UP);
	REG32(GBASE(PINMUX) + GOFFSET(PINMUX, DIOB0_SEL)) =
		GC_PINMUX_I2C0_SCL_SEL;
	REG32(GBASE(PINMUX) + GOFFSET(PINMUX, DIOB1_SEL)) =
		GC_PINMUX_I2C0_SDA_SEL;
}

static void deferred_ite_sync_reset(void)
{
	/* Enable writing to the long life register */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 1);
	GREG32(PMU, LONG_LIFE_SCRATCH1) |= BOARD_ITE_EC_SYNC_NEEDED;
	/* Disable writing to the long life register */
	GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 0);

	system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED |
		     SYSTEM_RESET_HARD);
}
DECLARE_DEFERRED(deferred_ite_sync_reset);

#define CROS_CMD_ITE_SYNC    0
static int ite_sync_preparer(void *data_in, size_t in_size,
			     void *data_out, size_t out_size)
{

	if (in_size != 1)
		return USB_I2C_WRITE_COUNT_INVALID;

	if (*((uint8_t *)data_in) != CROS_CMD_ITE_SYNC)
		return USB_I2C_UNSUPPORTED_COMMAND;

	if (!ccd_is_cap_enabled(CCD_CAP_EC_FLASH))
		return USB_I2C_DISABLED;

	/* Let the usb reply to make it to the host. */
	hook_call_deferred(&deferred_ite_sync_reset_data, 10 * MSEC);

	return 0;
}

static void register_ite_sync(void)
{
	usb_i2c_register_cros_cmd_handler(ite_sync_preparer);
}

DECLARE_HOOK(HOOK_INIT, register_ite_sync, HOOK_PRIO_DEFAULT);
