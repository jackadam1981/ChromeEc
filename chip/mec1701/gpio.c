/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for MEC1701 */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "lpc_chip.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)


/* MEC17XX
 * board level, for example glados has gpio.inc
 * GPIO_INT(LID_OPEN,   PIN(027), GPIO_INT_BOTH | GPIO_PULL_UP, lid_interrupt)
 * GPIO_INT(AC_PRESENT, PIN(030), GPIO_INT_BOTH, extpower_interrupt)
 * GPIO_INT(WP_L, PIN(033), GPIO_INT_BOTH,        switch_interrupt)
 * ...
 * GPIO(PCH_SLP_S0_L, PIN(0211), GPIO_INPUT | GPIO_PULL_DOWN)
 * GPIO(PD_RST_L,     PIN(0130), GPIO_ODR_HIGH)
 * GPIO(USB2_OTG_ID,  PIN(013),  GPIO_ODR_LOW)
 * GPIO(I2C0_0_SCL,   PIN(015),  GPIO_INPUT)
 *
 * The PIN macro is from ?
 * Online master repository has include/gpio_list.h
 * #define PIN(a, b...) static const int _pin_ ## a ## _ ## b \
 *   __attribute__((unused, section(".unused"))) = __LINE__
 *
 * But platform/ec/include/gpio_list.h does not define PIN()
 * #define GPIO(name, pin, flags) {#name, GPIO_##pin, flags},
 * so entries in gpio.inc expand to
 * GPIO(I2C0_0_SCL,            PIN(015),  GPIO_INPUT)
 * { I2C0_0_SCL, GPIO_PIN(015), GPIO_INPUT }
 * Macro GPIO_PIN() is defined in platform/ec/chip/mec17xx/config_chip.h
 * #define GPIO_PIN(index) (index / 10), (1 << (index % 10))
 * #define GPIO_PIN_MASK(pin, mask) (pin), (mask)
 *
 * GPIO_PIN(015) expands to (15 / 10), (1 << (15 mod 10)) = 5, 32
 * which is GPIO Bank 1 bit 5
 * Google has decided to define GPIO's in banks of 10. They are taking
 * the Microchip octal GPIO numbering as base 10 and turning it into
 * bank of 10.
 * GPIO 015 in hardware is bank 0 bit 15
 * Should we change these macros to reflect the hardware?
 *
 */

struct gpio_int_mapping {
	int8_t girq_id;
	int8_t port_offset;
};

/* Mapping from GPIO port to GIRQ info
 * MEC1701 each bank contains 32 GPIO's. Pin Id is the bit position [0:31]
 * Bank		GPIO's		GIRQ
 * 0		0000 - 0036	11
 * 1		0040 - 0076	10
 * 2		0100 - 0135	9
 * 3		0140 - 0175	8
 * 4		0200 - 0235	12
 * 5		0240 - 0276	26
 */
static const struct gpio_int_mapping int_map[6] = {
	{ 11, 0 }, { 10, 1 }, { 9, 2 },
	{ 8, 3 }, { 12, 4 }, { 26, 5 }
};



/*
 * NOTE: GCC __builtin_ffs(val) returns (index + 1) of least significant
 * 1-bit of val or if val == 0 returns 0
 */
void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	int i;
	uint32_t val;

	while (mask) {
		i = __builtin_ffs(mask) - 1;
		val = MEC17XX_GPIO_CTL(port, i);
		val &= ~((1 << 12) | (1 << 13));
		/* mux_control = 0 indicates GPIO */
		if (func > 0)
			val |= (func & 0x3) << 12;
		MEC17XX_GPIO_CTL(port, i) = val;
		mask &= ~(1 << i);
	}
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	uint32_t mask = gpio_list[signal].mask;
	int i;
	uint32_t val;

	if (mask == 0)
		return 0;
	i = GPIO_MASK_TO_NUM(mask);
	val = MEC17XX_GPIO_CTL(gpio_list[signal].port, i);

	return (val & (1 << 24)) ? 1 : 0;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	uint32_t mask = gpio_list[signal].mask;
	int i;

	if (mask == 0)
		return;
	i = GPIO_MASK_TO_NUM(mask);

	if (value)
		MEC17XX_GPIO_CTL(gpio_list[signal].port, i) |= (1 << 16);
	else
		MEC17XX_GPIO_CTL(gpio_list[signal].port, i) &= ~(1 << 16);
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	int i;
	uint32_t val;

	while (mask) {
		i = GPIO_MASK_TO_NUM(mask);
		mask &= ~(1 << i);
		val = MEC17XX_GPIO_CTL(port, i);

		/*
		 * Select open drain first, so that we don't glitch the signal
		 * when changing the line to an output.
		 */
		if (flags & GPIO_OPEN_DRAIN)
			val |= (1 << 8);
		else
			val &= ~(1 << 8);

		if (flags & GPIO_OUTPUT) {
			val |= (1 << 9);
			val &= ~(1 << 10);
		} else {
			val &= ~(1 << 9);
			val |= (1 << 10);
		}

		/* Handle pullup / pulldown */
		if (flags & GPIO_PULL_UP)
			val = (val & ~0x3) | 0x1;
		else if (flags & GPIO_PULL_DOWN)
			val = (val & ~0x3) | 0x2;
		else
			val &= ~0x3;

		/* Set up interrupt */
		if (flags & (GPIO_INT_F_RISING | GPIO_INT_F_FALLING))
			val |= (1 << 7);
		else
			val &= ~(1 << 7);

		val &= ~(0x7 << 4);

		if ((flags & GPIO_INT_F_RISING) && (flags & GPIO_INT_F_FALLING))
			val |= 0x7 << 4;
		else if (flags & GPIO_INT_F_RISING)
			val |= 0x5 << 4;
		else if (flags & GPIO_INT_F_FALLING)
			val |= 0x6 << 4;
		else if (flags & GPIO_INT_F_HIGH)
			val |= 0x1 << 4;
		else if (!(flags & GPIO_INT_F_LOW)) /* No interrupt flag set */
			val |= 0x4 << 4;

		/* Set up level */
		if (flags & GPIO_HIGH)
			val |= (1 << 16);
		else if (flags & GPIO_LOW)
			val &= ~(1 << 16);

		MEC17XX_GPIO_CTL(port, i) = val;
	}
}

/*
 * gpio_list[signal].port = [0, 6] each port contains up to 32 pins
 * gpio_list[signal].mask = bit mask in 32-bit port
 * NOTE: MEC17xx GPIO are always aggregated not direct connected to NVIC.
 * GPIO's are aggregated into banks of 32 pins.
 * Each bank/port are connected to a GIRQ.
 * int_map[port].girq_id is the GIRQ ID
 * The bit number in the GIRQ registers is the same as the bit number
 * in the GPIO bank.
 */
int gpio_enable_interrupt(enum gpio_signal signal)
{
	int i, port, girq_id;

	if (gpio_list[signal].mask == 0)
		return EC_SUCCESS;

	i = GPIO_MASK_TO_NUM(gpio_list[signal].mask);
	port = gpio_list[signal].port;
	girq_id = int_map[port].girq_id;

	MEC17XX_INT_ENABLE(girq_id) = (1 << i);
	MEC17XX_INT_BLK_EN |= (1 << girq_id);

	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	int i, port, girq_id;

	if (gpio_list[signal].mask == 0)
		return EC_SUCCESS;

	i = GPIO_MASK_TO_NUM(gpio_list[signal].mask);
	port = gpio_list[signal].port;
	girq_id = int_map[port].girq_id;


	MEC17XX_INT_DISABLE(girq_id) = (1 << i);

	return EC_SUCCESS;
}

/*
 * MEC17xx Interrupt Source is R/W1C no need for read-modify-write.
 * GPIO's are aggregated meaning the NVIC Pending bit may be
 * set for another GPIO in the GIRQ. You can clear NVIC pending
 * and the hardware should re-assert it within one Cortex-M4 clock.
 * If the Cortex-M4 is clocked slower than AHB then the Cortex-M4
 * will take longer to register the interrupt. Not clearing NVIC
 * pending leave a pending status if only the GPIO this routine
 * clears is pending.
 * NVIC (system control) register space is strongly-ordered
 * Interrupt Aggregator is in Device space (system bus connected
 * to AHB) with the Cortex-M4 write buffer.
 * We need to insure the write to aggregator register in device
 * AHB space completes before NVIC pending is cleared.
 * The Cortex-M4 memory ordering rules imply Device access
 * comes before strongly ordered access. Cortex-M4 will not re-order
 * the writes. Due to the presence of the write buffer a DSB will
 * not guarantee the clearing of the device status completes. Add
 * a read back before clearing NVIC pending.
 * GIRQ 8, 9, 10, 11, 12, 26 map to NVIC inputs 0, 1, 2, 3, 4, and 18.
 */
int gpio_clear_pending_interrupt(enum gpio_signal signal)
{
	int i, port, girq_id;

	if (gpio_list[signal].mask == 0)
		return EC_SUCCESS;

	i = GPIO_MASK_TO_NUM(gpio_list[signal].mask);
	port = gpio_list[signal].port;
	girq_id = int_map[port].girq_id;

	/* Clear interrupt source sticky status bit even if not enabled */
	MEC17XX_INT_SOURCE(girq_id) = (1 << i);
	i = MEC17XX_INT_SOURCE(girq_id);
	task_clear_pending_irq(girq_id - 8);

	return EC_SUCCESS;
}

/*
 * MCHP NOTE - called from main before scheduler started
*/
void gpio_pre_init(void)
{
	int i;
	int flags;
	int is_warm = system_is_reboot_warm();
	const struct gpio_info *g = gpio_list;


	for (i = 0; i < GPIO_COUNT; i++, g++) {
		flags = g->flags;

		if (flags & GPIO_DEFAULT)
			continue;

		/*
		 * If this is a warm reboot, don't set the output levels or
		 * we'll shut off the AP.
		 */
		if (is_warm)
			flags &= ~(GPIO_LOW | GPIO_HIGH);

		gpio_set_flags_by_mask(g->port, g->mask, flags);

		/* Use as GPIO, not alternate function */
		gpio_set_alternate_function(g->port, g->mask, -1);
	}
}

/* Clear any interrupt flags before enabling GPIO interrupt
 * Original code has flaws.
 * Writing result register to source only clears bits that have their
 * enable and sources bits set.
 * We must clear the NVIC pending R/W bit before setting NVIC enable.
 * NVIC Pending is only cleared by the NVIC HW on ISR entry.
 * Modifications are:
 * 1. Clear all status bits in each GPIO GIRQ. This assumes any edges
 *    will occur after gpio_init. The old code is also making this assumption
 *    for the GPIO's that have been enabled.
 * 2. Clear NVIC pending to prevent ISR firing on false edge.
*/
#define ENABLE_GPIO_GIRQ(x) \
	do { \
		MEC17XX_INT_SOURCE(x) = 0xfffffffful; \
		task_clear_pending_irq(MEC17XX_IRQ_GIRQ ## x); \
		task_enable_irq(MEC17XX_IRQ_GIRQ ## x); \
	} while (0)


static void gpio_init(void)
{
	ENABLE_GPIO_GIRQ(8);
	ENABLE_GPIO_GIRQ(9);
	ENABLE_GPIO_GIRQ(10);
	ENABLE_GPIO_GIRQ(11);
	ENABLE_GPIO_GIRQ(12);
	ENABLE_GPIO_GIRQ(26);
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Interrupt handlers */


/**
 * Handler for each GIRQ interrupt. This reads and clears the interrupt bits for
 * the GIRQ interrupt, then finds and calls the corresponding GPIO interrupt
 * handlers.
 *
 * @param girq		GIRQ index
 * @param port_offset	GPIO port offset for the given GIRQ
 */
static void gpio_interrupt(int girq)
{
	int i, bit;
	const struct gpio_info *g = gpio_list;
	uint32_t sts = MEC17XX_INT_RESULT(girq);

    /* CPRINTS("MEC1701 GPIO GIRQ %d result = 0x%08x", girq, sts); */
	trace12(0, GPIO, 0, "GPIO GIRQ %d result = 0x%08x", girq, sts);

	/* RW1C, no need for read-modify-write */
	MEC17XX_INT_SOURCE(girq) = sts;

	for (i = 0; i < GPIO_IH_COUNT && sts; ++i, ++g) {
		bit = __builtin_ffs(g->mask) - 1;
		if (sts & (1 << bit))
			gpio_irq_handlers[i](i);
		sts &= ~(1 << bit);
	}
}

#define GPIO_IRQ_FUNC(irqfunc, girq) \
	void irqfunc(void) \
	{ \
		gpio_interrupt(girq); \
	}

GPIO_IRQ_FUNC(__girq_8_interrupt, 8);
GPIO_IRQ_FUNC(__girq_9_interrupt, 9);
GPIO_IRQ_FUNC(__girq_10_interrupt, 10);
GPIO_IRQ_FUNC(__girq_11_interrupt, 11);
GPIO_IRQ_FUNC(__girq_12_interrupt, 12);
GPIO_IRQ_FUNC(__girq_26_interrupt, 26);

#undef GPIO_IRQ_FUNC

/*
 * Declare IRQs.  Nesting this macro inside the GPIO_IRQ_FUNC macro works
 * poorly because DECLARE_IRQ() stringizes its inputs.
 */
DECLARE_IRQ(MEC17XX_IRQ_GIRQ8, __girq_8_interrupt, 1);
DECLARE_IRQ(MEC17XX_IRQ_GIRQ9, __girq_9_interrupt, 1);
DECLARE_IRQ(MEC17XX_IRQ_GIRQ10, __girq_10_interrupt, 1);
DECLARE_IRQ(MEC17XX_IRQ_GIRQ11, __girq_11_interrupt, 1);
DECLARE_IRQ(MEC17XX_IRQ_GIRQ12, __girq_12_interrupt, 1);
DECLARE_IRQ(MEC17XX_IRQ_GIRQ26, __girq_26_interrupt, 1);
