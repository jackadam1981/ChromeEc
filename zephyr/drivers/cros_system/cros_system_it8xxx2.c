/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT ite_it8xxx2_cros_gctrl

#include <device.h>
#include <drivers/cros_system.h>
#include <logging/log.h>
#include <soc.h>
#include <soc/ite_it8xxx2/reg_def_cros.h>

#include "system.h"

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_DBG);

#define GCTRL_IT8XXX2_REG_BASE \
	((struct gctrl_reg *)DT_INST_REG_ADDR(0))

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
	int chip_reset_cause = 0;
	uint8_t raw_reset_cause = gctrl_base->GCTRL_RSTS & IT8XXX2_GCTRL_LRS;
	uint8_t raw_reset_cause2 = gctrl_base->GCTRL_SPCTRL4 &
		(IT8XXX2_GCTRL_LRSIWR | IT8XXX2_GCTRL_LRSIPWRSWTR |
		IT8XXX2_GCTRL_LRSIPGWR);

	/* Clear reset cause. */
	gctrl_base->GCTRL_RSTS |= IT8XXX2_GCTRL_LRS;
	gctrl_base->GCTRL_SPCTRL4 |= (IT8XXX2_GCTRL_LRSIWR |
		IT8XXX2_GCTRL_LRSIPWRSWTR | IT8XXX2_GCTRL_LRSIPGWR);

	LOG_DBG("get_reset_cause");
	/* Determine if watchdog reset or power on reset. */
	if (raw_reset_cause & 0x02) {
		LOG_DBG("WATCHDOG_RST");
		chip_reset_cause = WATCHDOG_RST;
	} else if (raw_reset_cause & 0x01) {
		LOG_DBG("POWERUP");
		chip_reset_cause = POWERUP;
	} else {
		if ((gctrl_base->GCTRL_RSTS & IT8XXX2_GCTRL_VCCDO) == 0x80) {
			LOG_DBG("POWERUP2");
			chip_reset_cause = POWERUP;
		}
	}

	if (raw_reset_cause2 & IT8XXX2_GCTRL_LRSIWR) {
		LOG_DBG("VCC1_RST_PIN");
		chip_reset_cause = VCC1_RST_PIN;
	}

	return chip_reset_cause;
}

static int cros_system_it8xxx2_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int cros_system_it8xxx2_soc_reset(const struct device *dev)
{
	struct wdt_it8xxx2_regs *const wdt_base = WDT_IT8XXX2_REG_BASE;

	/* Disable interrupts to avoid task swaps during reboot. */
	interrupt_disable_all();

	/*
	 * Writing invalid key to watchdog module triggers a soft or hardware
	 * reset. It depends on the setting of bit0 at ETWDUARTCR register.
	 */
	wdt_base->ETWCFG |= IT8XXX2_WDT_EWDKEYEN;
	wdt_base->EWDKEYR = 0x00;

	/* Spin and wait for reboot */
	while (1)
		;

	/* Should never return */
	return 0;
}

static int cros_system_it8xxx2_hibernate(const struct device *dev,
					 uint32_t seconds,
					 uint32_t microseconds)
{
	struct wdt_it8xxx2_regs *const wdt_base = WDT_IT8XXX2_REG_BASE;

	int i;

	/* disable all interrupts */
	interrupt_disable_all();

	/* bit5: watchdog is disabled. */
	wdt_base->ETWCTRL |= IT8XXX2_WDT_EWDSCEN;

	/*
	 * Setup GPIOs for hibernate.  On some boards, it's possible that this
	 * may not return at all.  On those boards, power to the EC is likely
	 * being turn off entirely.
	 */
	if (board_hibernate_late) {
		/*
		 * Set reset flag in case board_hibernate_late() doesn't
		 * return.
		 */
		chip_save_reset_flags(EC_RESET_FLAG_HIBERNATE);
		board_hibernate_late();
	}

	if (seconds || microseconds) {
		/* At least 1 ms for hibernate. */
		uint64_t c = (seconds * 1000 + microseconds / 1000 + 1) * 1024;

		//uint64divmod(&c, 1000);
		/* enable a 56-bit timer and clock source is 1.024 KHz */
		//ext_timer_stop(FREE_EXT_TIMER_L, 1);
		//ext_timer_stop(FREE_EXT_TIMER_H, 1);
		//IT83XX_ETWD_ETXPSR(FREE_EXT_TIMER_L) = EXT_PSR_1P024K;
		//IT83XX_ETWD_ETXPSR(FREE_EXT_TIMER_H) = EXT_PSR_1P024K_HZ;
		//IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_L) = c & 0xffffff;
		//IT83XX_ETWD_ETXCNTLR(FREE_EXT_TIMER_H) = (c >> 24) & 0xffffffff;
		//ext_timer_start(FREE_EXT_TIMER_H, 1);
		//ext_timer_start(FREE_EXT_TIMER_L, 0);
	}


	//for (i = 0; i < hibernate_wake_pins_used; ++i)
	//	gpio_enable_interrupt(hibernate_wake_pins[i]);

	chip_pll_ctrl(CHIP_PLL_SLEEP);
	//interrupt_enable();
	/* standby instruction */
	//clock_cpu_standby();

	/* we should never reach that point */
	//__builtin_unreachable();

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
	      CONFIG_CROS_SYSTEM_IT8XXX2_INIT_PRIORITY,
	      &cros_system_driver_it8xxx2_api);

