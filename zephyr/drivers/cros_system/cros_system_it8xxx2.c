/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/cros_system.h>
#include <logging/log.h>
#include <soc.h>
#include <soc/ite_it8xxx2/reg_def_cros.h>

#include "system.h"

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);


#define GCTRL_NODE DT_INST(0, ite_it8xxx2_cros_gctrl)
#define GCTRL_IT8XXX2_REG_BASE \
	((struct gctrl_reg *)DT_REG_ADDR(GCTRL_NODE))

#define WDT_NODE DT_INST(0, ite_it8xxx2_watchdog)
#define WDT_IT8XXX2_REG_BASE \
	((struct wdt_it8xxx2_regs *)DT_REG_ADDR(WDT_NODE))

static const char *cros_system_it8xxx2_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "ite";
}

static uint32_t system_get_chip_id(void)
{
	struct gctrl_reg *const gctrl_base = GCTRL_IT8XXX2_REG_BASE;

	return (gctrl_base->GCTRL_ECHIPID1 << 16) |
		(gctrl_base->GCTRL_ECHIPID2 << 8) |
		gctrl_base->GCTRL_ECHIPID3;

}

static uint8_t system_get_chip_version(void)
{
	struct gctrl_reg *const gctrl_base = GCTRL_IT8XXX2_REG_BASE;

	/* bit[3-0], chip version */
	return gctrl_base->GCTRL_ECHIPVER & 0x0F;
}

static char to_hex(int x)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	return 'a' + x - 10;
}

static const char *cros_system_it8xxx2_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[8] = {'i', 't'};
	int num = 4;
	uint32_t chip_id = system_get_chip_id();

	for (int n = 2; num >= 0; n++, num--)
		buf[n] = to_hex(chip_id >> (num * 4) & 0xF);

	return buf;
}

static const char *cros_system_it8xxx2_get_chip_revision(const struct device
							 *dev)
{
	ARG_UNUSED(dev);

	static char buf[3];
	uint8_t rev = system_get_chip_version();

	buf[0] = to_hex(rev + 0xa);
	buf[1] = 'x';
	buf[2] = '\0';

	return buf;
}

static int cros_system_it8xxx2_get_reset_cause(const struct device *dev)
{
	ARG_UNUSED(dev);
	struct gctrl_reg *const gctrl_base = GCTRL_IT8XXX2_REG_BASE;

	uint8_t raw_reset_cause = gctrl_base->GCTRL_RSTS & 0x03;
	//uint8_t raw_reset_cause2 = IT83XX_GCTRL_SPCTRL4 & 0x07;
	/* TODO */

	/* Clear reset cause. */
	gctrl_base->GCTRL_RSTS |= 0x03;
	gctrl_base->GCTRL_SPCTRL4 |= 0x07;

	return raw_reset_cause;
}

static int cros_system_it8xxx2_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int cros_system_it8xxx2_soc_reset(const struct device *dev)
{
	struct wdt_it8xxx2_regs *const wdt_base = WDT_IT8XXX2_REG_BASE;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

	/*
	 * TODO: watchdog reload
	 */

	/*
	 * Writing invalid key to watchdog module triggers a soft or hardware
	 * reset. It depends on the setting of bit0 at ETWDUARTCR register.
	 */
	wdt_base->ETWCFG |= 0x20;
	wdt_base->EWDKEYR = 0x00;

	/* Spin and wait for reboot; should never return */
	while (1) {
		;
	}

	/* should never return */
	return 0;
}

static int cros_system_it8xxx2_hibernate(const struct device *dev,
					 uint32_t seconds,
					 uint32_t microseconds)
{
	/*
	 * TODO:
	 */
	/* chip specific standby mode */
	//__enter_hibernate(seconds, microseconds);

	return 0;
}

static const struct cros_system_driver_api cros_system_driver_it8xxx2_api = {
	.get_reset_cause = cros_system_it8xxx2_get_reset_cause,
	.soc_reset = cros_system_it8xxx2_soc_reset,
	.hibernate = cros_system_it8xxx2_hibernate,
	.chip_vendor = cros_system_it8xxx2_get_chip_vendor,
	.chip_name = cros_system_it8xxx2_get_chip_name,
	.chip_revision = cros_system_it8xxx2_get_chip_revision,
};

DEVICE_DEFINE(cros_system_it8xxx2_0, "CROS_SYSTEM", cros_system_it8xxx2_init,
	      NULL, NULL, NULL, PRE_KERNEL_1,
	      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
	      &cros_system_driver_it8xxx2_api);

