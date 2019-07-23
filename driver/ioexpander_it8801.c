/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "driver/ioexpander_it8801.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "ioexpander.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "registers.h"
#include <stddef.h>
#include "task.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ## args)

static int i2c_init_done;

/*
 * Initialize the raw keyboard interface.
 */
void keyboard_raw_init(void)
{
	/*
	 * The I/O expander communicated with EC is through
	 * I2C, but the I2C is not ready during initialization
	 * of the keyboard raw. So this function can not do
	 * anything.
	 */
}

/*
 * Finish initialization after task scheduling has started.
 */
void keyboard_raw_task_start(void)
{
	/* KSO alternate function switching(KSO[21:18]) */
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_GPIO00_KSO19, IT8801_REG_MASK_GPIOAFS_FUNC2);
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_GPIO01_KSO18, IT8801_REG_MASK_GPIOAFS_FUNC2);
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_GPIO22_KSO21, IT8801_REG_MASK_GPIOAFS_FUNC2);
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_GPIO23_KSO20, IT8801_REG_MASK_GPIOAFS_FUNC2);

	if (IS_ENABLED(CONFIG_KEYBOARD_COL2_INVERTED))
		/* KSO[2] is high, others are low. */
		i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR, IT8801_REG_MASK_KSOSDIC |
			IT8801_REG_MASK_SELKSO2);
	else
		/* KSO[21:18,12:11,6:0] pins low. */
		i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR, IT8801_REG_MASK_AKSOSC);

	/* Keyboard scan in interrupt enable register */
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_KSIIER, 0xff);
	/* Gather KSI interrupt enable */
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_GIECR, IT8801_REG_MASK_GKSIIE);
	/* Alert response enable */
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_SMBCR, IT8801_REG_MASK_ARE);

	keyboard_raw_enable_interrupt(1);
}

/* Column mapping to KSO of IT8801 for customer */
#ifndef CONFIG_KEYBOARD_KSO_IT8801
static const uint8_t kso_mapping[] = {
	0, 1, 2, 3, 4, 5,
	6, 17, 18, 16, 15, 11,
	12, 13, 14, 0xff, 0xff, 0xff
};
BUILD_ASSERT(ARRAY_SIZE(kso_mapping) == 18);
#endif

/*
 * Drive the specified column low.
 */
test_mockable void keyboard_raw_drive_column(int col)
{
	int kso_val;

	if (!i2c_init_done)
		return;

	/* Tri-state all outputs */
	if (col == KEYBOARD_COLUMN_NONE)
		/* KSO[21:18,12:11,6:0] output high */
		kso_val = IT8801_REG_MASK_KSOSDIC | IT8801_REG_MASK_AKSOSC;
	/* Assert all outputs */
	else if (col == KEYBOARD_COLUMN_ALL)
		/* KSO[21:18,12:11,6:0] output low */
		kso_val = IT8801_REG_MASK_AKSOSC;
	/* Selected KSO output low, all others KSO pull high. */
	else
		kso_val = kso_mapping[col];

	if (IS_ENABLED(CONFIG_KEYBOARD_COL2_INVERTED))
		/* KSO[2] is inverted. */
		i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR, kso_val ^= IT8801_REG_MASK_SELKSO2);
	else
		i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR, kso_val);
}

/*
 * Read raw row state.
 * Bits are 1 if signal is present, 0 if not present.
 */
test_mockable int keyboard_raw_read_rows(void)
{
	int data, ksieer;

	if (!i2c_init_done)
		return 0;

	i2c_read8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_KSIDR, &data);

	/* This register needs to write clear after reading data */
	i2c_read8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_KSIEER, &ksieer);
	i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		IT8801_REG_KSIEER, ksieer);

	/* Bits are active-low, so invert returned levels */
	return data ^ 0xff;
}

/*
 * Enable or disable keyboard matrix scan interrupts.
 */
void keyboard_raw_enable_interrupt(int enable)
{
	if (!i2c_init_done)
		return;

	if (enable) {
		i2c_write8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
			IT8801_REG_KSIEER, 0xff);
		gpio_clear_pending_interrupt(GPIO_IT8801_SMB_INT);
		gpio_enable_interrupt(GPIO_IT8801_SMB_INT);
	} else {
		gpio_disable_interrupt(GPIO_IT8801_SMB_INT);
	}
}

/*
 * Interrupt handler for keyboard matrix scan interrupt.
 */
void io_expander_it8801_interrupt(enum gpio_signal signal)
{
	/* Wake the scan task */
	task_wake(TASK_ID_KEYSCAN);
}

/*
 * Check the I2C function has been initialized.
 */
static void i2c_init_check(void)
{
	i2c_init_done = 1;
}
DECLARE_HOOK(HOOK_INIT, i2c_init_check, HOOK_PRIO_INIT_I2C + 1);

/*
 *Initialize the general purpose I/O port(GPIO)
 */
#ifdef CONFIG_IO_EXPANDER
static int it8801_ioex_init(int ioex)
{
	int i2c_port, i2c_addr;

	i2c_port = ioex_config[ioex].i2c_host_port;
	i2c_addr = ioex_config[ioex].i2c_slave_addr;

	/* GPIO alternate function switching(GPIO[23:22, 01:00])*/
	i2c_write8(i2c_port, i2c_addr, IT8801_REG_GPIO00_KSO19,
		IT8801_REG_MASK_GPIOAFS_FUNC1);
	i2c_write8(i2c_port, i2c_addr, IT8801_REG_GPIO01_KSO18,
		IT8801_REG_MASK_GPIOAFS_FUNC1);
	i2c_write8(i2c_port, i2c_addr, IT8801_REG_GPIO22_KSO21,
		IT8801_REG_MASK_GPIOAFS_FUNC1);
	i2c_write8(i2c_port, i2c_addr, IT8801_REG_GPIO23_KSO20,
		IT8801_REG_MASK_GPIOAFS_FUNC1);

	return 0;
}

static int it8801_ioex_check_is_valid(int chip_info, int port, int mask)
{
	if (chip_info == IT8801_CHIP_INFO) {
		switch (port) {
		case 0:
			if (mask & ~IT8801_VALID_GPIO_G0_MASK) {
				CPRINTF("GPIO0%d is not support in IT8801\n",
					__fls(mask));
				return EC_ERROR_INVAL;
			}
			break;
		case 1:
			if (mask & ~IT8801_VALID_GPIO_G1_MASK) {
				CPRINTF("GPIO1%d is not support in IT8801\n",
					__fls(mask));
				return EC_ERROR_INVAL;
			}
			break;
		case 2:
			if (mask & ~IT8801_VALID_GPIO_G2_MASK) {
				CPRINTF("GPIO2%d is not support in IT8801\n",
					__fls(mask));
				return EC_ERROR_INVAL;
			}
			break;
		default:
			CPRINTF("Port%d is not support in IT8801\n", port);
			break;
		}
	} else {
		CPRINTF("This chip is not support\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int it8801_ioex_get_level(int ioex, int port, int mask, int *val)
{
	int rv, reg, i2c_port, i2c_addr, chip_info;

	i2c_port = ioex_config[ioex].i2c_host_port;
	i2c_addr = ioex_config[ioex].i2c_slave_addr;
	chip_info = ioex_config[ioex].chip_info;

	rv = it8801_ioex_check_is_valid(chip_info, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	reg = IT8801_REG_GPIO_DATA_IN(port);
	rv = i2c_read8(i2c_port, i2c_addr, reg, val);

	*val = !!(*val & mask);

	return rv;
}

static int it8801_ioex_set_level(int ioex, int port, int mask, int value)
{
	int rv, reg, val, i2c_port, i2c_addr, chip_info;

	i2c_port = ioex_config[ioex].i2c_host_port;
	i2c_addr = ioex_config[ioex].i2c_slave_addr;
	chip_info = ioex_config[ioex].chip_info;

	rv = it8801_ioex_check_is_valid(chip_info, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	reg = IT8801_REG_GPIO_DATA_OUT(port);

	rv = i2c_read8(i2c_port, i2c_addr, reg, &val);
	if (value)
		val |= mask;
	else
		val &= ~mask;
	rv |= i2c_write8(i2c_port, i2c_addr, reg, val);

	return rv;
}

static int it8801_ioex_get_flags_by_mask(int ioex, int port,
	int mask, int *flags)
{
	int rv, reg, val, mask_sh, i2c_port, i2c_addr, chip_info;
	int pin = 0;

	i2c_port = ioex_config[ioex].i2c_host_port;
	i2c_addr = ioex_config[ioex].i2c_slave_addr;
	chip_info = ioex_config[ioex].chip_info;

	rv = it8801_ioex_check_is_valid(chip_info, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	mask_sh = mask;
	while (!(mask_sh & 0x01)) {
		mask_sh >>= 1;
		pin += 1;
	}

	reg = IT8801_REG_GPIOXXCR(port) + pin;

	/* Select open drain 0:push-pull 1:open-drain */
	rv = i2c_read8(i2c_port, i2c_addr, reg, &val);
	if (val & IT8801_REG_GPIODIR)
		*flags |= GPIO_OUTPUT;
	else
		*flags |= GPIO_INPUT;

	/* Select GPIO direction */
	rv |= i2c_read8(i2c_port, i2c_addr,	reg, &val);
	if (val & IT8801_REG_GPIOOT)
		*flags |= GPIO_OPEN_DRAIN;

	reg = IT8801_REG_GPIO_DATA_IN(port);

	/* Configure the output level */
	rv |= i2c_read8(i2c_port, i2c_addr,	reg, &val);
	if (val & mask)
		*flags |= GPIO_HIGH;
	else
		*flags |= GPIO_LOW;

	return rv;
}

static int it8801_ioex_set_flags_by_mask(int ioex, int port, int mask,
		int flags)
{
	int rv, reg, flg, val, mask_sh, i2c_port, i2c_addr, chip_info;
	int pin = 0;

	i2c_port = ioex_config[ioex].i2c_host_port;
	i2c_addr = ioex_config[ioex].i2c_slave_addr;
	chip_info = ioex_config[ioex].chip_info;

	rv = it8801_ioex_check_is_valid(chip_info, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	flg = flags & ~IT8801_SUPPORT_GPIO_FLAGS;
	if (flg) {
		ccprintf("Flag 0x%08x is not supported\n", flg);
		return EC_ERROR_INVAL;
	}

	mask_sh = mask;
	while (!(mask_sh & 0x01)) {
		mask_sh >>= 1;
		pin += 1;
	}

	reg = IT8801_REG_GPIOXXCR(port) + pin;

	/* Select open drain 0:push-pull 1:open-drain */
	rv = i2c_read8(i2c_port, i2c_addr, reg, &val);
	if (flags & GPIO_OPEN_DRAIN)
		val |= IT8801_REG_GPIOOT;
	else
		val &= ~IT8801_REG_GPIOOT;
	rv |= i2c_write8(i2c_port, i2c_addr, reg, val);

	/* Select GPIO direction */
	rv |= i2c_read8(i2c_port, i2c_addr, reg, &val);
	if (flags & GPIO_OUTPUT)
		val |= IT8801_REG_GPIODIR;
	else
		val &= ~IT8801_REG_GPIODIR;
	rv |= i2c_write8(i2c_port, i2c_addr, reg, val);

	reg = IT8801_REG_GPIO_DATA_OUT(port);

	/* Configure the output level */
	rv |= i2c_read8(i2c_port, i2c_addr, reg, &val);
	if (flags & GPIO_HIGH)
		val |= mask;
	else if (flags & GPIO_LOW)
		val &= ~mask;
	rv |= i2c_write8(i2c_port, i2c_addr, reg, val);

	return rv;
}

const struct ioexpander_drv it8801_ioexpander_drv = {
	.init              = &it8801_ioex_init,
	.get_level         = &it8801_ioex_get_level,
	.set_level         = &it8801_ioex_set_level,
	.get_flags_by_mask = &it8801_ioex_get_flags_by_mask,
	.set_flags_by_mask = &it8801_ioex_set_flags_by_mask,
};
#endif /* CONFIG_IO_EXPANDER */

static void dump_register(int reg)
{
	int rv;
	int data;

	ccprintf("[%Xh] = ", reg);

	rv = i2c_read8(I2C_PORT_IO_EXPANDER_IT8801, IT8801_REG_ADDR,
		reg, &data);

	if (!rv)
		ccprintf("0x%02x\n", data);
	else
		ccprintf("ERR (%d)\n", rv);
}

static int it8801_dump(int argc, char **argv)
{
	dump_register(IT8801_REG_KSIIER);
	dump_register(IT8801_REG_KSIEER);
	dump_register(IT8801_REG_KSIDR);
	dump_register(IT8801_REG_KSOMCR);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(it8801_dump, it8801_dump, "NULL",
			"Dumps IT8801 registers");
