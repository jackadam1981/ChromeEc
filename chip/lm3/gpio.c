/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* 0-terminated list of GPIO base addresses */
static const uint32_t gpio_bases[] = {
	LM3_GPIO_A, LM3_GPIO_B, LM3_GPIO_C, LM3_GPIO_D,
	LM3_GPIO_E, 0
};

/**
 * Find the index of a GPIO port base address
 *
 * This is used by the clock gating registers.
 *
 * @param port_base	Base address to find (LM3_GPIO_[A-E])
 *
 * @return The index, or -1 if no match.
 */
static int find_gpio_port_index(uint32_t port_base)
{
	int i;

	for (i = 0; gpio_bases[i]; i++) {
		if (gpio_bases[i] == port_base)
			return i;
	}

	return -1;
}

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	int port_index = find_gpio_port_index(port);
	uint32_t cgmask;
	uint32_t offset;

	/* Ignore (do nothing for) invalid port values */
	if (port_index < 0)
		return;

	/* Enable the GPIO port in run and sleep. */
	cgmask = 1 << port_index;
	switch (cgmask) {
	case 1:	/* CGC_OFFSET_GPIOA */
		offset = CGC_OFFSET_GPIOA;
		break;
	case 2: /* CGC_OFFSET_GPIOB */
		offset = CGC_OFFSET_GPIOB;
		break;
	case 4: /* CGC_OFFSET_GPIOC */
		offset = CGC_OFFSET_GPIOC;
		break;
	case 8: /* CGC_OFFSET_GPIOD */
		offset = CGC_OFFSET_GPIOD;
		break;
	case 16: /* CGC_OFFSET_GPIOE */
		offset = CGC_OFFSET_GPIOE;
		break;
	}
	clock_enable_peripheral(offset, cgmask,
			CGC_MODE_RUN | CGC_MODE_SLEEP);

	if (func >= 0) {
		int pctlmask = 0;
		int i;

		/* Expand mask from bits to nibbles */
		for (i = 0; i < 8; i++) {
			if (mask & (1 << i))
				pctlmask |= 1 << (4 * i);
		}

		LM3_GPIO_PCTL(port) =
			(LM3_GPIO_PCTL(port) & ~(pctlmask * 0xf)) |
			(pctlmask * func);
		LM3_GPIO_AFSEL(port) |= mask;
	} else {
		LM3_GPIO_AFSEL(port) &= ~mask;
	}
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	return LM3_GPIO_DATA(gpio_list[signal].port,
			     gpio_list[signal].mask) ? 1 : 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	/*
	 * Ok to write 0xff because LM3_GPIO_DATA bit-masks only the bit
	 * we care about.
	 */
	LM3_GPIO_DATA(gpio_list[signal].port,
		      gpio_list[signal].mask) = (value ? 0xff : 0);
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	/*
	 * Select open drain first, so that we don't glitch the signal
	 * when changing the line to an output.
	 */
	if (flags & GPIO_OPEN_DRAIN)
		LM3_GPIO_ODR(port) |= mask;
	else
		LM3_GPIO_ODR(port) &= ~mask;

	if (flags & GPIO_OUTPUT)
		LM3_GPIO_DIR(port) |= mask;
	else
		LM3_GPIO_DIR(port) &= ~mask;

	/* Handle pullup / pulldown */
	if (flags & GPIO_PULL_UP) {
		LM3_GPIO_PUR(port) |= mask;
	} else if (flags & GPIO_PULL_DOWN) {
		LM3_GPIO_PDR(port) |= mask;
	} else {
		/* No pull up/down */
		LM3_GPIO_PUR(port) &= ~mask;
		LM3_GPIO_PDR(port) &= ~mask;
	}

	/* Set up interrupt type */
	if (flags & (GPIO_INT_F_LOW | GPIO_INT_F_HIGH))
		LM3_GPIO_IS(port) |= mask;
	else
		LM3_GPIO_IS(port) &= ~mask;

	if (flags & (GPIO_INT_F_RISING | GPIO_INT_F_HIGH))
		LM3_GPIO_IEV(port) |= mask;
	else
		LM3_GPIO_IEV(port) &= ~mask;

	/* Handle interrupting on both edges */
	if ((flags & GPIO_INT_F_RISING) &&
	    (flags & GPIO_INT_F_FALLING))
		LM3_GPIO_IBE(port) |= mask;
	else
		LM3_GPIO_IBE(port) &= ~mask;

	if (flags & GPIO_ANALOG)
		LM3_GPIO_DEN(port) &= ~mask;
	else
		LM3_GPIO_DEN(port) |= mask;

	/* Set level */
	if (flags & GPIO_HIGH)
		LM3_GPIO_DATA(port, mask) = 0xff;
	else if (flags & GPIO_LOW)
		LM3_GPIO_DATA(port, mask) = 0;
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;
	/* Fail if no interrupt handler */
	if (signal >= GPIO_IH_COUNT)
		return EC_ERROR_UNKNOWN;

	LM3_GPIO_IM(g->port) |= g->mask;
	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;

	/* Fail if no interrupt handler */
	if (signal >= GPIO_IH_COUNT)
		return EC_ERROR_UNKNOWN;

	LM3_GPIO_IM(g->port) &= ~g->mask;
	return EC_SUCCESS;
}

int gpio_clear_pending_interrupt(enum gpio_signal signal)
{
	const struct gpio_info *g = gpio_list + signal;

	/* Fail if no interrupt handler */
	if (signal >= GPIO_IH_COUNT)
		return EC_ERROR_INVAL;

	LM3_GPIO_ICR(g->port) |= g->mask;
	return EC_SUCCESS;
}

#ifdef CONFIG_LOW_POWER_IDLE
/**
 * Convert GPIO port to a mask that can be used to set the
 * clock gate control register for GPIOs.
 */
static int gpio_port_to_clock_gate_mask(uint32_t gpio_port)
{
	int index = find_gpio_port_index(gpio_port);

	return index >= 0 ? (1 << index) : 0;
}
#endif

void gpio_pre_init(void)
{
	const struct gpio_info *g = gpio_list;
	int is_warm = 0;
	int i;

	/*
	 * Enable clocks to all the GPIO blocks since we use all of
	 * them as GPIOs in run and sleep modes.
	 */
	clock_enable_peripheral(CGC_OFFSET_GPIOA, 1,
				CGC_MODE_RUN | CGC_MODE_SLEEP);
	clock_enable_peripheral(CGC_OFFSET_GPIOB, 1,
				CGC_MODE_RUN | CGC_MODE_SLEEP);
	clock_enable_peripheral(CGC_OFFSET_GPIOC, 1,
				CGC_MODE_RUN | CGC_MODE_SLEEP);
	clock_enable_peripheral(CGC_OFFSET_GPIOD, 1,
				CGC_MODE_RUN | CGC_MODE_SLEEP);
	clock_enable_peripheral(CGC_OFFSET_GPIOE, 1,
				CGC_MODE_RUN | CGC_MODE_SLEEP);

	/*
	 * Disable GPIO commit control for PD7 and PF0, since we don't use the
	 * NMI pin function.
	 */
	LM3_GPIO_LOCK(LM3_GPIO_D) = LM3_GPIO_LOCK_UNLOCK;
	LM3_GPIO_CR(LM3_GPIO_D) |= 0x80;
	LM3_GPIO_LOCK(LM3_GPIO_D) = 0;

	/* Clear SSI0 alternate function on PA2:5 */
	LM3_GPIO_AFSEL(LM3_GPIO_A) &= ~0x3c;

	/* Mask all GPIO interrupts */
	for (i = 0; gpio_bases[i]; i++)
		LM3_GPIO_IM(gpio_bases[i]) = 0;

	/* Set all GPIOs to defaults */
	for (i = 0; i < GPIO_COUNT; i++, g++) {
		int flags = g->flags;

		if (flags & GPIO_DEFAULT)
			continue;

#ifdef CONFIG_LOW_POWER_IDLE
		/*
		 * Enable board specific GPIO ports to interrupt deep sleep by
		 * providing a clock to that port in deep sleep mode.
		 */
		if (flags & GPIO_INT_DSLEEP) {
			clock_enable_peripheral(CGC_OFFSET_GPIO,
				gpio_port_to_clock_gate_mask(g->port),
				CGC_MODE_ALL);
		}
#endif

		/*
		 * If this is a warm reboot, don't set the output levels or
		 * we'll shut off the main chipset.
		 */
		if (is_warm)
			flags &= ~(GPIO_LOW | GPIO_HIGH);

		/* Set up GPIO based on flags */
		gpio_set_flags_by_mask(g->port, g->mask, flags);

		/* Use as GPIO, not alternate function */
		gpio_set_alternate_function(g->port, g->mask, -1);
	}

#ifdef CONFIG_LOW_POWER_IDLE
	/*
	 * Enable KB scan row to interrupt deep sleep by providing a clock
	 * signal to that port in deep sleep mode.
	 */
	clock_enable_peripheral(CGC_OFFSET_GPIO,
				gpio_port_to_clock_gate_mask(KB_SCAN_ROW_GPIO),
				CGC_MODE_ALL);
#endif
}

/*
 * List of GPIO IRQs to enable. Don't automatically enable interrupts for
 * the keyboard input GPIO bank - that's handled separately. Of course the
 * bank is different for different systems.
 */
static const uint8_t gpio_irqs[] = {
	LM3_IRQ_GPIOA, LM3_IRQ_GPIOB, LM3_IRQ_GPIOC, LM3_IRQ_GPIOD,
	LM3_IRQ_GPIOE
};

static void gpio_init(void)
{
	int i;

	/* Enable IRQs now that pins are set up */
	for (i = 0; i < ARRAY_SIZE(gpio_irqs); i++)
		task_enable_irq(gpio_irqs[i]);
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Interrupt handlers */

/**
 * Handle a GPIO interrupt.
 *
 * @param port		GPIO port (LM3_GPIO_*)
 * @param mis		Masked interrupt status value for that port
 */
static void gpio_interrupt(int port, uint32_t mis)
{
	int i = 0;
	const struct gpio_info *g = gpio_list;

	for (i = 0; i < GPIO_IH_COUNT && mis; i++, g++) {
		if (port == g->port && (mis & g->mask)) {
			gpio_irq_handlers[i](i);
			mis &= ~g->mask;
		}
	}
}

/**
 * Handlers for each GPIO port.  These read and clear the interrupt bits for
 * the port, then call the master handler above.
 */
#define GPIO_IRQ_FUNC(irqfunc, gpiobase)		\
	void irqfunc(void)				\
	{						\
		uint32_t mis = LM3_GPIO_MIS(gpiobase);	\
		LM3_GPIO_ICR(gpiobase) = mis;		\
		gpio_interrupt(gpiobase, mis);		\
	}

GPIO_IRQ_FUNC(__gpio_a_interrupt, LM3_GPIO_A);
GPIO_IRQ_FUNC(__gpio_b_interrupt, LM3_GPIO_B);
GPIO_IRQ_FUNC(__gpio_c_interrupt, LM3_GPIO_C);
GPIO_IRQ_FUNC(__gpio_d_interrupt, LM3_GPIO_D);
GPIO_IRQ_FUNC(__gpio_e_interrupt, LM3_GPIO_E);

#undef GPIO_IRQ_FUNC

/*
 * Declare IRQs.  Nesting this macro inside the GPIO_IRQ_FUNC macro works
 * poorly because DECLARE_IRQ() stringizes its inputs.
 */
DECLARE_IRQ(LM3_IRQ_GPIOA, __gpio_a_interrupt, 1);
DECLARE_IRQ(LM3_IRQ_GPIOB, __gpio_b_interrupt, 1);
DECLARE_IRQ(LM3_IRQ_GPIOC, __gpio_c_interrupt, 1);
DECLARE_IRQ(LM3_IRQ_GPIOD, __gpio_d_interrupt, 1);
DECLARE_IRQ(LM3_IRQ_GPIOE, __gpio_e_interrupt, 1);
