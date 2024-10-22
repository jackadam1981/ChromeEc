/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT ite_it8xxx2_gctrl

#include "drivers/cros_system.h"
#include "gpio/gpio_int.h"
#include "system.h"
#include "util.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <soc.h>
#include <soc/ite_it8xxx2/reg_def_cros.h>

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

#define GCTRL_IT8XXX2_REG_BASE \
	((struct gctrl_it8xxx2_regs *)DT_INST_REG_ADDR(0))

#define WDT_IT8XXX2_REG_BASE \
	((struct wdt_it8xxx2_regs *)DT_REG_ADDR(DT_NODELABEL(twd0)))

static const char *cros_system_it8xxx2_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "ite";
}

static uint32_t system_get_chip_id(void)
{
	struct gctrl_ite_ec_regs *const gctrl_base = GCTRL_ITE_EC_REGS_BASE;

	return (gctrl_base->GCTRL_ECHIPID1 << 16) |
	       (gctrl_base->GCTRL_ECHIPID2 << 8) | gctrl_base->GCTRL_ECHIPID3;
}

static uint8_t system_get_chip_version(void)
{
	struct gctrl_ite_ec_regs *const gctrl_base = GCTRL_ITE_EC_REGS_BASE;

	/* bit[3-0], chip version */
	return gctrl_base->GCTRL_ECHIPVER & 0x0F;
}

static const char *cros_system_it8xxx2_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[8] = { 'i', 't' };
	uint32_t chip_id = system_get_chip_id();
	int num = 4;

	for (int n = 2; num >= 0; n++, num--)
		snprintf(buf + n, (sizeof(buf) - n), "%x",
			 chip_id >> (num * 4) & 0xF);

	return buf;
}

static const char *
cros_system_it8xxx2_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[3];
	uint8_t rev = system_get_chip_version();

	snprintf(buf, sizeof(buf), "%cx", rev + 'a');

	return buf;
}

static int cros_system_it8xxx2_get_reset_cause(const struct device *dev)
{
	ARG_UNUSED(dev);
	struct gctrl_ite_ec_regs *const gctrl_base = GCTRL_ITE_EC_REGS_BASE;
	uint8_t last_reset_source = gctrl_base->GCTRL_RSTS & IT51XXX_GCTRL_LRS;
	uint8_t raw_reset_cause2 =
		gctrl_base->GCTRL_SPCTRL4 &
		(IT51XXX_GCTRL_LRSIWR | IT51XXX_GCTRL_LRSIPWRSWTR |
		 IT51XXX_GCTRL_LRSIPGWR);

	/* Clear reset cause. */
	gctrl_base->GCTRL_RSTS |= IT51XXX_GCTRL_LRS;
	gctrl_base->GCTRL_SPCTRL4 |=
		(IT51XXX_GCTRL_LRSIWR | IT51XXX_GCTRL_LRSIPWRSWTR |
		 IT51XXX_GCTRL_LRSIPGWR);

	if (last_reset_source & IT51XXX_GCTRL_IWDTR) {
		return WATCHDOG_RST;
	}
	if (raw_reset_cause2 & IT51XXX_GCTRL_LRSIWR) {
		/*
		 * We can't differentiate between power-on and reset pin because
		 * LRSIWR is set on both ~WRST assertion and power-on, and LRS
		 * is either 0 or 1 in both cases.
		 *
		 * Some EC code paths care about only one of these options,
		 * so we force both causes to be reported (via
		 * system_set_reset_flags() behind our caller's back) even
		 * though in reality it had to be only one of them because
		 * being unable to report a hard reset breaks some
		 * functionality, as would being unable to report power-on
		 * reset.
		 */
		system_set_reset_flags(EC_RESET_FLAG_RESET_PIN);
		return POWERUP;
	}
	return UNKNOWN_RST;
}

static int cros_system_it8xxx2_init(const struct device *dev)
{
#ifdef CONFIG_SOC_SERIES_IT8XXX2
	struct gctrl_it8xxx2_regs *const gctrl_base = GCTRL_IT8XXX2_REG_BASE;

	/* System triggers a soft reset by default (command: reboot). */
	gctrl_base->GCTRL_ETWDUARTCR &= ~IT8XXX2_GCTRL_ETWD_HW_RST_EN;
#endif
	return 0;
}

static mm_reg_t wdt_base = DT_REG_ADDR(DT_NODELABEL(twd0));
/* 0x87: External WDT Key */
#define REG_EWDKEYR  0x07
/* 0x81: External Timer1/WDT Configuration */
#define REG_ETWCFG   0x01
#define WDT_EWDKEYEN BIT(5)
/* 0x85: External Timer1/WDT Control */
#define REG_ETWCTRL  0x05
#define WDT_EWDSCEN  BIT(5)

static int cros_system_it8xxx2_soc_reset(const struct device *dev)
{
	//struct gctrl_it51xxx_regs *const gctrl_base = GCTRL_IT51XXX_REGS_BASE;
	//struct wdt_it8xxx2_regs *const wdt_base = WDT_IT8XXX2_REG_BASE;
	uint32_t chip_reset_flags = chip_read_reset_flags();
	uint8_t reg_val;

	/* Disable interrupts to avoid task swaps during reboot. */
	interrupt_disable_all();

	if (chip_reset_flags & (EC_RESET_FLAG_HARD | EC_RESET_FLAG_HIBERNATE))
		//gctrl_base->GCTRL_ETWDUARTCR |= IT8XXX2_GCTRL_ETWD_HW_RST_EN;

	/*
	 * Writing invalid key to watchdog module triggers a soft or hardware
	 * reset. It depends on the setting of bit0 at ETWDUARTCR register.
	 */
	//wdt_base->ETWCFG |= IT8XXX2_WDT_EWDKEYEN;
	//wdt_base->EWDKEYR = 0x00;
	reg_val = sys_read8(wdt_base + REG_ETWCFG);
	sys_write8(reg_val | WDT_EWDKEYEN, wdt_base + REG_ETWCFG);
	sys_write8(0, wdt_base + REG_EWDKEYR);

	/* Spin and wait for reboot */
	while (1)
		;

	/* Should never return */
	return 0;
}

/*
 * Fake wake ISR handler, needed for pins that do not have a handler.
 */
void wake_isr(enum gpio_signal signal)
{
}

static mm_reg_t timer_base = DT_REG_ADDR(DT_NODELABEL(timer));
/* 0x10, 0x18, 0x20, 0x28, 0x30, 0x38: External Timer 3-8 Control Register (n=0 to 5) */
#define CROS_TIMER_ETNCTRL(n)   (0x10 + ((n) * 8))
#define CROS_TIMER_ETCOMB       BIT(3)
#define CROS_TIMER_ETNRST       BIT(1)
#define CROS_TIMER_ETNEN        BIT(0)
/* 0x11, 0x19, 0x21, 0x29, 0x31, 0x39: External Timer 3-8 Prescaler Register (n=0 to 5) */
#define CROS_TIMER_ETNPSR(n)    (0x11 + ((n) * 8))
/* 0x14, 0x1c, 0x24, 0x2c, 0x34, 0x3c: External Timer 3-8 Counter Register (n=0 to 5) */
#define CROS_TIMER_ETNCNTLLR(n) (0x14 + ((n) * 8))
/* 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c: External Timer 3-8 Counter Observation Register (n=0 to 5) */
#define CROS_TIMER_ETNCNTOLR(n) (0x48 + ((n) * 4))

#define FREE_RUN_TIMER            EXT_TIMER_4
#define FREE_RUN_TIMER_IRQ        DT_IRQ_BY_IDX(DT_NODELABEL(timer), 1, irq)
/* Free run timer max count is 36.4 hr (base on clock source 32768Hz) */
#define FREE_RUN_TIMER_MAX_CNT    0xFFFFFFFFUL
enum ext_timer_idx {
	EXT_TIMER_3 = 0, /* Event timer */
	EXT_TIMER_4,     /* Free run timer */
	EXT_TIMER_5,     /* Busy wait low timer */
	EXT_TIMER_6,     /* Busy wait high timer */
	EXT_TIMER_7,
	EXT_TIMER_8,
};
enum ext_clk_src_sel {
	EXT_PSR_32P768K = 0,
	EXT_PSR_1P024K,
	EXT_PSR_32,
	EXT_PSR_EC_CLK,
};

static int cros_system_it8xxx2_hibernate(const struct device *dev,
					 uint32_t seconds,
					 uint32_t microseconds)
{
	//struct wdt_it8xxx2_regs *const wdt_base = WDT_IT8XXX2_REG_BASE;

	/* Disable all interrupts. */
	interrupt_disable_all();

	/* Save and disable interrupts */
	ite_intc_save_and_disable_interrupts();

	/* bit5: watchdog is disabled. */
	//wdt_base->ETWCTRL |= IT8XXX2_WDT_EWDSCEN;
	uint8_t reg_val = sys_read8(wdt_base + REG_ETWCTRL);
	sys_write8(reg_val | WDT_EWDSCEN, wdt_base + REG_ETWCTRL);

	/*
	 * Setup GPIOs for hibernate. On some boards, it's possible that this
	 * may not return at all. On those boards, power to the EC is likely
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
		/*
		 * Convert milliseconds(or at least 1 ms) to 32 Hz
		 * free run timer count for hibernate.
		 */
		uint8_t etnctrl;
		uint32_t c =
			(seconds * 1000 + microseconds / 1000 + 1) * 32 / 1000;

		/* Enable a 32-bit timer and clock source is 32 Hz */
		/* Disable external timer x */
		//IT8XXX2_EXT_CTRLX(FREE_RUN_TIMER) &= ~IT8XXX2_EXT_ETXEN;
		etnctrl = sys_read8(timer_base + CROS_TIMER_ETNCTRL(FREE_RUN_TIMER));
		sys_write8(etnctrl & ~CROS_TIMER_ETNEN,
			   timer_base + CROS_TIMER_ETNCTRL(EXT_TIMER_4));
		irq_disable(FREE_RUN_TIMER_IRQ);
		//IT8XXX2_EXT_PSRX(FREE_RUN_TIMER) = EXT_PSR_32;
		sys_write8(EXT_PSR_32, timer_base + CROS_TIMER_ETNPSR(FREE_RUN_TIMER));
		//IT8XXX2_EXT_CNTX(FREE_RUN_TIMER) = c & FREE_RUN_TIMER_MAX_CNT;
		sys_write32(c & FREE_RUN_TIMER_MAX_CNT,
			    timer_base + CROS_TIMER_ETNCNTLLR(FREE_RUN_TIMER));
		/* Enable and re-start external timer x */
		//IT8XXX2_EXT_CTRLX(FREE_RUN_TIMER) |=
		//	(IT8XXX2_EXT_ETXEN | IT8XXX2_EXT_ETXRST);
		etnctrl = sys_read8(timer_base + CROS_TIMER_ETNCTRL(FREE_RUN_TIMER));
		sys_write8(etnctrl | CROS_TIMER_ETNRST | CROS_TIMER_ETNEN,
			   timer_base + CROS_TIMER_ETNCTRL(FREE_RUN_TIMER));

		irq_enable(FREE_RUN_TIMER_IRQ);
	}

#ifdef CONFIG_PLATFORM_EC_HIBERNATE_WAKE_PINS

/*
 * Get the interrupt DTS node for this wakeup pin
 */
#define WAKEUP_INT(id, prop, idx) DT_PHANDLE_BY_IDX(id, prop, idx)

/*
 * Get the named-gpio node for this wakeup pin by reading the
 * irq-gpio property from the interrupt node.
 */
#define WAKEUP_NGPIO(id, prop, idx) \
	DT_PHANDLE(WAKEUP_INT(id, prop, idx), irq_pin)

/*
 * Reset and re-enable interrupts on this wake pin.
 */
#define WAKEUP_SETUP(id, prop, idx)                                     \
	do {                                                            \
		gpio_pin_configure_dt(                                  \
			GPIO_DT_FROM_NODE(WAKEUP_NGPIO(id, prop, idx)), \
			GPIO_INPUT);                                    \
		gpio_enable_dt_interrupt(                               \
			GPIO_INT_FROM_NODE(WAKEUP_INT(id, prop, idx))); \
	} while (0);

	/*
	 * For all the wake-pins, re-init the GPIO and re-enable the interrupt.
	 */
	DT_FOREACH_PROP_ELEM(SYSTEM_DT_NODE_HIBERNATE_CONFIG, wakeup_irqs,
			     WAKEUP_SETUP);

#undef WAKEUP_INT
#undef WAKEUP_NGPIO
#undef WAKEUP_SETUP

#endif /* CONFIG_PLATFORM_EC_HIBERNATE_WAKE_PINS */

	/* EC sleep mode */
	chip_pll_ctrl(CHIP_PLL_SLEEP);

	/* Chip sleep and wait timer wake it up */
	__asm__ volatile("wfi");

	/* Reset EC when wake up from sleep mode (system hibernate) */
	system_reset(SYSTEM_RESET_HIBERNATE);

	return 0;
}

static DEVICE_API(cros_system, cros_system_driver_it8xxx2_api) = {
	.get_reset_cause = cros_system_it8xxx2_get_reset_cause,
	.soc_reset = cros_system_it8xxx2_soc_reset,
	.hibernate = cros_system_it8xxx2_hibernate,
	.chip_vendor = cros_system_it8xxx2_get_chip_vendor,
	.chip_name = cros_system_it8xxx2_get_chip_name,
	.chip_revision = cros_system_it8xxx2_get_chip_revision,
};

#if CONFIG_CROS_SYSTEM_IT8XXX2_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif
DEVICE_DEFINE(cros_system_it8xxx2_0, "CROS_SYSTEM", cros_system_it8xxx2_init,
	      NULL, NULL, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_IT8XXX2_INIT_PRIORITY,
	      &cros_system_driver_it8xxx2_api);
