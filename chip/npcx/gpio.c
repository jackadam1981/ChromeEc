/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "gpio_chip.h"
#include "keyboard_config.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "system.h"
#include "system_chip.h"
#include "lpc_chip.h"
#include "ec_commands.h"
#include "host_command.h"

#if !(DEBUG_GPIO)
#define CPUTS(...)
#define CPRINTS(...)
#else
#define CPUTS(outstr) cputs(CC_GPIO, outstr)
#define CPRINTS(format, args...) cprints(CC_GPIO, format, ## args)
#endif

struct npcx_wui {
	uint8_t table : 2;
	uint8_t group : 3;
	uint8_t bit   : 3;
};

/* Constants for GPIO interrupt mapping */
#define GPIO_INT(name, pin, flags, signal) NPCX_WUI_GPIO_##pin,
static const struct npcx_wui gpio_wui_table[] = {
	#include "gpio.wrap"
};

struct npcx_gpio {
	uint8_t port  : 4;
	uint8_t bit   : 3;
	uint8_t valid : 1;
};

BUILD_ASSERT(sizeof(struct npcx_gpio) == 1);

struct npcx_alt {
	uint8_t group     : 4;
	uint8_t bit       : 3;
	uint8_t inverted  : 1;
};

struct gpio_alt_map {
	struct npcx_gpio gpio;
	struct npcx_alt  alt;
};

BUILD_ASSERT(sizeof(struct gpio_alt_map) == 2);

/* Constants for GPIO alternative mapping */
const struct gpio_alt_map gpio_alt_table[] = NPCX_ALT_TABLE;

struct gpio_lvol_item {
	struct npcx_gpio lvol_gpio[8];
};

/* Constants for GPIO low-voltage mapping */
const struct gpio_lvol_item gpio_lvol_table[] = NPCX_LVOL_TABLE;


/*****************************************************************************/
/* Internal functions */

static int gpio_match(uint8_t port, uint8_t mask, struct npcx_gpio gpio)
{
	return (gpio.valid && (gpio.port == port) && ((1 << gpio.bit) == mask));
}

static int gpio_alt_sel(uint8_t port, uint8_t bit, int8_t func)
{
	struct gpio_alt_map const *map;

	for (map = ARRAY_BEGIN(gpio_alt_table);
	     map < ARRAY_END(gpio_alt_table);
	     map++) {
		if (gpio_match(port, 1 << bit, map->gpio)) {
			uint8_t alt_mask = 1 << map->alt.bit;

			/*
			 * func < 0          -> GPIO functionality
			 * map->alt.inverted -> Set DEVALT bit for GPIO
			 */
			if ((func < 0) ^ map->alt.inverted)
				NPCX_DEVALT(map->alt.group) &= ~alt_mask;
			else
				NPCX_DEVALT(map->alt.group) |=  alt_mask;

			return 1;
		}
	}

	if (func > 0)
		CPRINTS("Warn! No alter func in port%d, pin%d", port, bit);

	return -1;
}

/* Set interrupt type for GPIO input */
static void gpio_interrupt_type_sel(enum gpio_signal signal, uint32_t flags)
{
	uint8_t table, group, pmask;

	if (signal >= ARRAY_SIZE(gpio_wui_table))
		return;

	table = gpio_wui_table[signal].table;
	group = gpio_wui_table[signal].group;
	pmask = 1 << gpio_wui_table[signal].bit;

	ASSERT(flags & GPIO_INT_ANY);

	/* Handle interrupt for level trigger */
	if ((flags & GPIO_INT_F_HIGH) || (flags & GPIO_INT_F_LOW)) {
		/* Set detection mode to level */
		NPCX_WKMOD(table, group) |= pmask;
		/* Handle interrupting on level high */
		if (flags & GPIO_INT_F_HIGH)
			NPCX_WKEDG(table, group) &= ~pmask;
		/* Handle interrupting on level low */
		else if (flags & GPIO_INT_F_LOW)
			NPCX_WKEDG(table, group) |= pmask;
	}
	/* Handle interrupt for edge trigger */
	else {
		/* Set detection mode to edge */
		NPCX_WKMOD(table, group) &= ~pmask;
		/* Handle interrupting on both edges */
		if ((flags & GPIO_INT_F_RISING) &&
			(flags & GPIO_INT_F_FALLING)) {
			/* Enable any edge */
			NPCX_WKAEDG(table, group) |= pmask;
		}
		/* Handle interrupting on rising edge */
		else if (flags & GPIO_INT_F_RISING) {
			/* Disable any edge */
			NPCX_WKAEDG(table, group) &= ~pmask;
			NPCX_WKEDG(table, group) &= ~pmask;
		}
		/* Handle interrupting on falling edge */
		else if (flags & GPIO_INT_F_FALLING) {
			/* Disable any edge */
			NPCX_WKAEDG(table, group) &= ~pmask;
			NPCX_WKEDG(table, group) |= pmask;
		}
	}

	/* Enable wake-up input sources */
	NPCX_WKINEN(table, group) |= pmask;
	/*
	 * Clear pending bit since it might be set
	 * if WKINEN bit is changed.
	 */
	NPCX_WKPCL(table, group) |= pmask;

	/* No support analog mode */
}

/* Select low voltage detection level */
void gpio_low_voltage_level_sel(uint8_t port, uint8_t mask, uint8_t low_voltage)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(gpio_lvol_table); i++) {
		const struct npcx_gpio *gpio = gpio_lvol_table[i].lvol_gpio;

		for (j = 0; j < ARRAY_SIZE(gpio_lvol_table[0].lvol_gpio); j++)
			if (gpio_match(port, mask, gpio[j])) {
				if (low_voltage)
					/* Select vol-detect level for 1.8V */
					SET_BIT(NPCX_LV_GPIO_CTL(i), j);
				else
					/* Select vol-detect level for 3.3V */
					CLEAR_BIT(NPCX_LV_GPIO_CTL(i), j);
				return;
			}

	}

	if (low_voltage)
		CPRINTS("Warn! No low voltage support in port%d, mask%d\n",
								port, mask);
}
/*
 * Make sure the bit depth of low voltage register.
 */
BUILD_ASSERT(ARRAY_SIZE(gpio_lvol_table[0].lvol_gpio) == 8);

/*****************************************************************************/
/* IC specific low-level driver */

void gpio_set_alternate_function(uint32_t port, uint32_t mask, int func)
{
	/* Enable alternative pins by func*/
	int pin;

	/* check each bit from mask  */
	for (pin = 0; pin < 8; pin++)
		if (mask & (1 << pin))
			gpio_alt_sel(port, pin, func);
}

test_mockable int gpio_get_level(enum gpio_signal signal)
{
	return !!(NPCX_PDIN(gpio_list[signal].port) & gpio_list[signal].mask);
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	if (value)
		NPCX_PDOUT(gpio_list[signal].port) |=  gpio_list[signal].mask;
	else
		NPCX_PDOUT(gpio_list[signal].port) &= ~gpio_list[signal].mask;
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	/* If all GPIO pins are locked, return directly */
#if defined(CHIP_FAMILY_NPCX7)
	if ((NPCX_PLOCK_CTL(port) & mask) == mask)
		return;
#endif

	/*
	 * Configure pin as input, if requested. Output is configured only
	 * after setting all other attributes, so as not to create a
	 * temporary incorrect logic state 0:input 1:output
	 */
	if (!(flags & GPIO_OUTPUT))
		NPCX_PDIR(port) &= ~mask;

	/* Select open drain 0:push-pull 1:open-drain */
	if (flags & GPIO_OPEN_DRAIN)
		NPCX_PTYPE(port) |= mask;
	else
		NPCX_PTYPE(port) &= ~mask;

	/* Select pull-up/down of GPIO 0:pull-up 1:pull-down */
	if (flags & GPIO_PULL_UP) {
		NPCX_PPUD(port)  &= ~mask;
		NPCX_PPULL(port) |= mask; /* enable pull down/up */
	} else if (flags & GPIO_PULL_DOWN) {
		NPCX_PPUD(port)  |= mask;
		NPCX_PPULL(port) |= mask; /* enable pull down/up */
	} else {
		/* No pull up/down */
		NPCX_PPULL(port) &= ~mask; /* disable pull down/up */
	}

	/* 1.8V low voltage select */
	if (flags & GPIO_SEL_1P8V) {
		/*
		 * Set IO type to open-drain & disable internal pulling
		 * before selecting low-voltage level
		 */
		NPCX_PTYPE(port) |= mask;
		NPCX_PPULL(port) &= ~mask;
		gpio_low_voltage_level_sel(port, mask, 1);
	} else
		gpio_low_voltage_level_sel(port, mask, 0);

	/* Set up interrupt type */
	if (flags & GPIO_INT_ANY) {
		const struct gpio_info *g = gpio_list;
		enum gpio_signal gpio_int;

		/* Find gpio signal in GPIO_INTs by port and mask */
		for (gpio_int = 0; gpio_int < GPIO_IH_COUNT; gpio_int++, g++)
			if ((g->port == port) && (g->mask & mask))
				gpio_interrupt_type_sel(gpio_int, flags);
	}

	/* Set level 0:low 1:high*/
	if (flags & GPIO_HIGH)
		NPCX_PDOUT(port) |= mask;
	else if (flags & GPIO_LOW)
		NPCX_PDOUT(port) &= ~mask;

	/* Configure pin as output, if requested 0:input 1:output */
	if (flags & GPIO_OUTPUT)
		NPCX_PDIR(port) |= mask;

	/* Lock GPIO output and configuration if need */
#if defined(CHIP_FAMILY_NPCX7)
	if (flags & GPIO_LOCKED)
		NPCX_PLOCK_CTL(port) |= mask;
#endif
}

int gpio_enable_interrupt(enum gpio_signal signal)
{
	struct npcx_wui wui;

	/* Fail if not an interrupt handler */
	if (signal >= GPIO_IH_COUNT)
		return EC_ERROR_PARAM1;

	wui = gpio_wui_table[signal];
	/* Set MIWU enable bit */
	NPCX_WKEN(wui.table, wui.group) |= 1 << wui.bit;

	return EC_SUCCESS;
}

int gpio_disable_interrupt(enum gpio_signal signal)
{
	struct npcx_wui wui;

	/* Fail if not an interrupt handler */
	if (signal >= GPIO_IH_COUNT)
		return EC_ERROR_PARAM1;

	wui = gpio_wui_table[signal];
	NPCX_WKEN(wui.table, wui.group) &= ~(1 << wui.bit);

	return EC_SUCCESS;
}

int gpio_clear_pending_interrupt(enum gpio_signal signal)
{
	struct npcx_wui wui;

	/* Fail if not an interrupt handler */
	if (signal >= GPIO_IH_COUNT)
		return EC_ERROR_PARAM1;

	wui = gpio_wui_table[signal];
	NPCX_WKPCL(wui.table, wui.group) |= 1 << wui.bit;

	return EC_SUCCESS;
}

void gpio_pre_init(void)
{
	const struct gpio_info *g = gpio_list;
	int is_warm;
	int flags;
	int i, j;

	system_check_bbram_on_reset();
	is_warm = system_is_reboot_warm();

#ifdef CHIP_FAMILY_NPCX7
	/*
	 * TODO: Set bit 7 of DEVCNT again for npcx7 series. Please see Errata
	 * for more information. It will be fixed in next chip.
	 */
	SET_BIT(NPCX_DEVCNT, 7);
	/* Lock VCC_RST# alternative bit in case switch to GPO77 unexpectedly */
	SET_BIT(NPCX_DEV_CTL4, NPCX_DEV_CTL4_VCC1_RST_LK);
#endif

	/* Pin_Mux for FIU/SPI (set to GPIO) */
	SET_BIT(NPCX_DEVALT(0), NPCX_DEVALT0_GPIO_NO_SPIP);
#if defined(NPCX_INT_FLASH_SUPPORT)
	SET_BIT(NPCX_DEVALT(0), NPCX_DEVALT0_NO_F_SPI);
#endif

	/* Pin_Mux for PWRGD */
	SET_BIT(NPCX_DEVALT(1), NPCX_DEVALT1_NO_PWRGD);

	/* Pin_Mux for PECI */
#ifndef CONFIG_PECI
	SET_BIT(NPCX_DEVALT(0xA), NPCX_DEVALTA_NO_PECI_EN);
#endif

	/* Pin_Mux for LPC & SHI */
#ifdef CONFIG_HOSTCMD_SPS
	/* Switching to eSPI mode for SHI interface */
	NPCX_DEVCNT |= 0x08;
	/* Alternate Intel bus interface LPC/eSPI to GPIOs first */
	SET_BIT(NPCX_DEVALT(ALT_GROUP_1), NPCX_DEVALT1_NO_LPC_ESPI);
#endif

	/* Clear all interrupt pending and enable bits of GPIOS */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 8; j++) {
			NPCX_WKPCL(i, j) = 0xFF;
			NPCX_WKEN(i, j) = 0;
		}
	}

	/* No support enable clock for the GPIO port in run and sleep. */
	/* Set flag for each GPIO pin in gpio_list */
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

		/* Set up GPIO based on flags */
		gpio_set_flags_by_mask(g->port, g->mask, flags);

		/*
		 * Ensure that any GPIO defined in gpio.inc is actually
		 * configured as a GPIO, and not left in its default state,
		 * which may or may not be as a GPIO.
		 */
		gpio_set_alternate_function(g->port, g->mask, -1);
	}
}

/* List of GPIO IRQs to enable. Don't automatically enable interrupts for
 * the keyboard input GPIO bank - that's handled separately. Of course the
 * bank is different for different systems. */
static void gpio_init(void)
{
	/* Enable IRQs now that pins are set up */
	task_enable_irq(NPCX_IRQ_MTC_WKINTAD_0);
	task_enable_irq(NPCX_IRQ_WKINTEFGH_0);
	task_enable_irq(NPCX_IRQ_WKINTC_0);
	task_enable_irq(NPCX_IRQ_TWD_WKINTB_0);
	task_enable_irq(NPCX_IRQ_WKINTA_1);
	task_enable_irq(NPCX_IRQ_WKINTB_1);
#ifndef HAS_TASK_KEYSCAN
	task_enable_irq(NPCX_IRQ_KSI_WKINTC_1);
#endif
	task_enable_irq(NPCX_IRQ_WKINTD_1);
	task_enable_irq(NPCX_IRQ_WKINTE_1);
	task_enable_irq(NPCX_IRQ_WKINTF_1);
	task_enable_irq(NPCX_IRQ_WKINTG_1);
	task_enable_irq(NPCX_IRQ_WKINTH_1);
#if defined(CHIP_FAMILY_NPCX7)
	task_enable_irq(NPCX_IRQ_WKINTFG_2);
#endif
}
DECLARE_HOOK(HOOK_INIT, gpio_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Interrupt handlers */

/**
 * Handle a GPIO interrupt.
 *
 * @param wui_int wui table & group for GPIO interrupt no.
 */

static void gpio_interrupt(struct npcx_wui wui_int)
{
	int i;
	uint8_t wui_mask;
	uint8_t table = wui_int.table;
	uint8_t group = wui_int.group;

	/* Get pending mask */
	wui_mask = NPCX_WKPND(table, group) & NPCX_WKEN(table, group);

	/* Find GPIOs and execute interrupt service routine */
	for (i = 0; i < GPIO_IH_COUNT && wui_mask; i++) {
		uint8_t pin_mask = 1 << gpio_wui_table[i].bit;

		if ((gpio_wui_table[i].table == table) &&
			(gpio_wui_table[i].group == group) &&
			(wui_mask & pin_mask)) {
			/* Clear pending bit of GPIO */
			NPCX_WKPCL(table, group) = pin_mask;
			/* Execute GPIO's ISR */
			gpio_irq_handlers[i](i);
			/* In case declare the same GPIO in gpio_wui_table */
			wui_mask &= ~pin_mask;
		}
	}

	if (wui_mask)
		/* No ISR for this interrupt, just clear it */
		NPCX_WKPCL(table, group) = wui_mask;
}

/**
 * Handlers for each GPIO port.  These read and clear the interrupt bits for
 * the port, then call the master handler above.
 */

#define GPIO_IRQ_FUNC(_irq_func, wui_int)		\
void _irq_func(void)					\
{							\
	gpio_interrupt(wui_int);			\
}

/* If we need to handle the other type interrupts except GPIO, add code here */
void __gpio_wk0efgh_interrupt(void)
{
#if defined(CONFIG_LPC) || defined(CONFIG_ESPI)
	/* Pending bit 7 or 6 or 5? */
	if (IS_BIT_SET(NPCX_WKEN(MIWU_TABLE_0 , MIWU_GROUP_5), 6) &&
	    IS_BIT_SET(NPCX_WKPND(MIWU_TABLE_0 , MIWU_GROUP_5), 6)) {
		/* Disable host wake-up */
		CLEAR_BIT(NPCX_WKEN(MIWU_TABLE_0, MIWU_GROUP_5), 6);
		/* Clear pending bit of WUI */
		SET_BIT(NPCX_WKPCL(MIWU_TABLE_0, MIWU_GROUP_5), 6);
	}
#ifdef CONFIG_ESPI
	else if (IS_BIT_SET(NPCX_WKEN(MIWU_TABLE_0, MIWU_GROUP_5), 5) &&
		 IS_BIT_SET(NPCX_WKPND(MIWU_TABLE_0, MIWU_GROUP_5), 5))
		espi_espirst_handler();
#else
	else if (IS_BIT_SET(NPCX_WKEN(MIWU_TABLE_0, MIWU_GROUP_5), 7) &&
		 IS_BIT_SET(NPCX_WKPND(MIWU_TABLE_0, MIWU_GROUP_5), 7))
		lpc_lreset_pltrst_handler();
#endif
	else
#endif
	{
		gpio_interrupt(WUI_INT(MIWU_TABLE_0, MIWU_GROUP_5));
		gpio_interrupt(WUI_INT(MIWU_TABLE_0, MIWU_GROUP_6));
		gpio_interrupt(WUI_INT(MIWU_TABLE_0, MIWU_GROUP_7));
		gpio_interrupt(WUI_INT(MIWU_TABLE_0, MIWU_GROUP_8));
	}
}

void __gpio_rtc_interrupt(void)
{
	/* Check pending bit 7 */
#ifdef CONFIG_HOSTCMD_RTC
	if (NPCX_WKPND(MIWU_TABLE_0, MIWU_GROUP_4) & 0x80) {
		/* Clear pending bit for WUI */
		SET_BIT(NPCX_WKPCL(MIWU_TABLE_0, MIWU_GROUP_4), 7);
		host_set_events(EC_HOST_EVENT_MASK(EC_HOST_EVENT_RTC));
	} else
#endif
	{
		gpio_interrupt(WUI_INT(MIWU_TABLE_0, MIWU_GROUP_1));
		gpio_interrupt(WUI_INT(MIWU_TABLE_0, MIWU_GROUP_4));
	}
}

void __gpio_wk1h_interrupt(void)
{
#if defined(CHIP_FAMILY_NPCX7) && defined(CONFIG_LOW_POWER_IDLE)
	/* Handle the interrupt from UART wakeup event */
	if (IS_BIT_SET(NPCX_WKEN(MIWU_TABLE_1, MIWU_GROUP_8), 7) &&
	    IS_BIT_SET(NPCX_WKPND(MIWU_TABLE_1, MIWU_GROUP_8), 7)) {
		/*
		 * Disable WKEN bit to avoid the other unnecessary interrupts
		 * from the coming data bits after the start bit. (Pending bit
		 * of CR_SIN is set when a high-to-low transaction occurs.)
		 */
		CLEAR_BIT(NPCX_WKEN(MIWU_TABLE_1, MIWU_GROUP_8), 7);
		/* Clear pending bit for WUI */
		SET_BIT(NPCX_WKPCL(MIWU_TABLE_1, MIWU_GROUP_8), 7);
		/* Notify the clock module that the console is in use. */
		clock_refresh_console_in_use();
	} else
#endif
		gpio_interrupt(WUI_INT(MIWU_TABLE_1, MIWU_GROUP_8));
}

GPIO_IRQ_FUNC(__gpio_wk0b_interrupt, WUI_INT(MIWU_TABLE_0, MIWU_GROUP_2));
GPIO_IRQ_FUNC(__gpio_wk0c_interrupt, WUI_INT(MIWU_TABLE_0, MIWU_GROUP_3));
GPIO_IRQ_FUNC(__gpio_wk1a_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_1));
GPIO_IRQ_FUNC(__gpio_wk1b_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_2));
#ifndef HAS_TASK_KEYSCAN
/* Delcare GPIO irq functions for KSI pins if there's no keyboard scan task, */
GPIO_IRQ_FUNC(__gpio_wk1c_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_3));
#endif
GPIO_IRQ_FUNC(__gpio_wk1d_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_4));
GPIO_IRQ_FUNC(__gpio_wk1e_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_5));
GPIO_IRQ_FUNC(__gpio_wk1f_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_6));
GPIO_IRQ_FUNC(__gpio_wk1g_interrupt, WUI_INT(MIWU_TABLE_1, MIWU_GROUP_7));
#if defined(CHIP_FAMILY_NPCX7)
GPIO_IRQ_FUNC(__gpio_wk2fg_interrupt, WUI_INT(MIWU_TABLE_2, MIWU_GROUP_6));
#endif

DECLARE_IRQ(NPCX_IRQ_MTC_WKINTAD_0, __gpio_rtc_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_TWD_WKINTB_0,  __gpio_wk0b_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_WKINTC_0,      __gpio_wk0c_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_WKINTEFGH_0,   __gpio_wk0efgh_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_WKINTA_1,      __gpio_wk1a_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_WKINTB_1,      __gpio_wk1b_interrupt, 2);
#ifndef HAS_TASK_KEYSCAN
DECLARE_IRQ(NPCX_IRQ_KSI_WKINTC_1,  __gpio_wk1c_interrupt, 2);
#endif
DECLARE_IRQ(NPCX_IRQ_WKINTD_1,      __gpio_wk1d_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_WKINTE_1,      __gpio_wk1e_interrupt, 2);
#ifdef CONFIG_HOSTCMD_SPS
/*
 * HACK: Make CS GPIO P1 to improve SHI reliability.
 * TODO: Increase CS-assertion-to-transaction-start delay on host to
 * accommodate P2 CS interrupt.
 */
DECLARE_IRQ(NPCX_IRQ_WKINTF_1,      __gpio_wk1f_interrupt, 1);
#else
DECLARE_IRQ(NPCX_IRQ_WKINTF_1,      __gpio_wk1f_interrupt, 2);
#endif
DECLARE_IRQ(NPCX_IRQ_WKINTG_1,      __gpio_wk1g_interrupt, 2);
DECLARE_IRQ(NPCX_IRQ_WKINTH_1,      __gpio_wk1h_interrupt, 2);
#if defined(CHIP_FAMILY_NPCX7)
DECLARE_IRQ(NPCX_IRQ_WKINTFG_2,     __gpio_wk2fg_interrupt, 2);
#endif

#undef GPIO_IRQ_FUNC

/* Get battery led gpio direction. */
static int gpio_get_dir_func(int argc, char **argv)
{
	int dir = 0;
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;
/*
 * Usage:
 * 	get_direction [0|1]
 *
 * 0: get gpio direction of BAT_LED_BLUE.
 * 1: get gpio direction of BAT_LED_AMBER.
 */

	if (!strcasecmp(argv[1], "0")) {
		/* Method 1: Front-End calling.
		dir = NPCX_PDIR(gpio_list[GPIO_BAT_LED_BLUE].port) &
			gpio_list[GPIO_BAT_LED_BLUE].mask; */

		/*
		 * Method 2: Back-End calling.
		 * BAT_LED_BLUE is PIN(8,0) => port = 0x8, mask = 0x1
		 * 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x02 (direction) = 0x40091002
		 */
		dir = REG8(0x40091002) & (0x1);

		if (dir)
			ccprintf("GPIO get direction of BAT_LED_BLUE is output\n");
		else
			ccprintf("GPIO get direction of BAT_LED_BLUE is input\n");
	} else if (!strcasecmp(argv[1], "1")) {
		/* Method 1: Front-End calling.
		dir = NPCX_PDIR(gpio_list[GPIO_BAT_LED_AMBER].port) &
			gpio_list[GPIO_BAT_LED_AMBER].mask; */

		/*
		 * Method 2: Back-End calling.
		 * BAT_LED_AMBER is PIN(C,4) => port = 0xC, mask = 0x10
		 * 0x40081000 (gpio base address) + 0xC (port) * 0x2000L (offset) + 0x02 (direction) = 0x40099002
		 */
		dir = REG8(0x40099002) & (0x10);

		if (dir)
			ccprintf("GPIO get direction of BAT_LED_AMBER is output\n");
		else
			ccprintf("GPIO get direction of BAT_LED_AMBER is input\n");
	} else {
		ccprintf("Input parameter is not [0|1]\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(get_direction, gpio_get_dir_func, "[0|1]",
		"GPIO get batt LED direction");

/* Set battery led gpio direction. */
static int gpio_set_dir_func(int argc, char **argv)
{
	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

/*
 * Usage:
 * 	set_direction [0|1] [0|1]
 *
 * First parameter:
 * 	0: BAT_LED_BLUE
 * 	1: BAT_LED_AMBER
 *
 * Second parameter:
 * 	0: set gpio direction to input.
 * 	1: set gpio direction to output.
 */

	if (!strcasecmp(argv[1], "0")) {
		if (!strcasecmp(argv[2], "0")) {
			/* Method 1: Front-End calling.
			NPCX_PDIR(gpio_list[GPIO_BAT_LED_BLUE].port)
				&= ~gpio_list[GPIO_BAT_LED_BLUE].mask; */

			/*
			 * Method 2: Back-End calling.
			 * BAT_LED_BLUE is PIN(8,0) => port = 0x8, mask = 0x1
			 * 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x02 (direction) = 0x40091002
			 */
			REG8(0x40091002) &= ~(0x1);

			ccprintf("GPIO set direction of BAT_LED_BLUE as input\n");
		} else if (!strcasecmp(argv[2], "1")) {
			/* Method 1: Front-End calling.
			NPCX_PDIR(gpio_list[GPIO_BAT_LED_BLUE].port)
				|= gpio_list[GPIO_BAT_LED_BLUE].mask; */

			/*
			 * Method 2: Back-End calling.
			 */
			REG8(0x40091002) |= (0x1);

			ccprintf("GPIO set direction of BAT_LED_BLUE as output\n");
		} else {
			ccprintf("Second input parameter is not [0|1]\n");
			return EC_ERROR_INVAL;
		}
	} else if (!strcasecmp(argv[1], "1")) {
		if (!strcasecmp(argv[2], "0")) {
			/* Method 1: Front-End calling.
			NPCX_PDIR(gpio_list[GPIO_BAT_LED_AMBER].port)
				&= ~gpio_list[GPIO_BAT_LED_AMBER].mask; */

			/*
			 * Method 2: Back-End calling.
			 * BAT_LED_AMBER is PIN(C,4) => port = 0xC, mask = 0x10
			 * 0x40081000 (gpio base address) + 0xC (port) * 0x2000L (offset) + 0x02 (direction) = 0x40099002
			 */
			REG8(0x40099002) &= ~(0x10);

			ccprintf("GPIO set direction of BAT_LED_AMBER as input\n");
		} else if (!strcasecmp(argv[2], "1")) {
			/* Method 1: Front-End calling.
			NPCX_PDIR(gpio_list[GPIO_BAT_LED_AMBER].port)
				|= gpio_list[GPIO_BAT_LED_AMBER].mask; */

			/*
			 * Method 2: Back-End calling.
			 */
			REG8(0x40099002) |= (0x10);

			ccprintf("GPIO set direction of BAT_LED_AMBER as output\n");
		} else {
			ccprintf("Second input parameter is not [0|1]\n");
			return EC_ERROR_INVAL;
		}

	} else {
		ccprintf("First input parameter is not [0|1]\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(set_direction, gpio_set_dir_func,
		"[0|1] [0|1]",
		"GPIO set batt LED direction");

/* Get battery led gpio output level. */
static int gpio_get_output_level_func(int argc, char **argv)
{
	int dir = 0;
	int level = 0;
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

/*
 * Usage:
 * 	get_out_level [0|1]
 *
 * 	0: Get gpio output level of BAT_LED_BLUE
 * 	1: Get gpio output level of BAT_LED_AMBER
 */

	if (!strcasecmp(argv[1], "0")) {
		/* Check if GPIO direction of BAT_LED_BLUE is output. */
		dir = REG8(0x40091002) & (0x1);
		if (dir) {
			ccprintf("GPIO get direction of BAT_LED_BLUE is output, ");
			/*
			 * BAT_LED_BLUE is PIN(8,0) => port = 0x8, mask = 0x1
			 * 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x00 (data out) = 0x40091000
			 */
			level = REG8(0x40091000) & (0x1);
			if (level)
				ccprintf("level is high.\n");
			else
				ccprintf("level is low.\n");
		}
		else {
			ccprintf("GPIO get direction of BAT_LED_BLUE is input\n");
			return EC_ERROR_INVAL;
		}
	} else if (!strcasecmp(argv[1], "1")) {
		/* Check if GPIO direction of BAT_LED_AMBER is output. */
		dir = REG8(0x40099002) & (0x10);
		if (dir) {
			ccprintf("GPIO get direction of BAT_LED_AMBER is output, ");
			/*
			 * BAT_LED_AMBER is PIN(C,4) => port = 0xC, mask = 0x10
			 * 0x40081000 (gpio base address) + 0xC (port) * 0x2000L (offset) + 0x00 (data out) = 0x40099000
			 */
			level = REG8(0x40099000) & (0x10);
			if (level)
				ccprintf("level is high.\n");
			else
				ccprintf("level is low.\n");
		}
		else {
			ccprintf("GPIO get direction of BAT_LED_AMBER is input\n");
			return EC_ERROR_INVAL;
		}
	} else {
		ccprintf("Input parameter is not [0|1]\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;

}
DECLARE_CONSOLE_COMMAND(get_out_level, gpio_get_output_level_func,
		"[0|1]",
		"GPIO get batt LED output level");

/* Set battery led gpio output level. */
static int gpio_set_output_level_func(int argc, char **argv)
{
	int dir = 0;
	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

/*
 * Usage:
 * 	set_out_level [0|1] [0|1]
 *
 * First parameter:
 * 	0: BAT_LED_BLUE
 * 	1: BAT_LED_AMBER
 *
 * Second parameter:
 * 	0: set gpio output level to low.
 * 	1: set gpio output level to high.
 */

	if (!strcasecmp(argv[1], "0")) {
		/* Check if GPIO direction of BAT_LED_BLUE is output. */
		dir = REG8(0x40091002) & (0x1);
		if (dir)
			ccprintf("GPIO get direction of BAT_LED_BLUE is output\n");
		else {
			ccprintf("GPIO get direction of BAT_LED_BLUE is input\n");
			return EC_ERROR_INVAL;
		}
		if (!strcasecmp(argv[2], "0")) {
			/*
			 * Set BAT_LED_BLUE to output low.
			 * BAT_LED_BLUE is PIN(8,0) => port = 0x8, mask = 0x1
			 * 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x00 (data out) = 0x40091000
			 */
			REG8(0x40091000) &= ~(0x1);

			ccprintf("Set BAT_LED_BLUE to ouput low.\n");
		} else if (!strcasecmp(argv[2], "1")) {
			/*
			 * Set BAT_LED_BLUE to output high.
			 */
			REG8(0x40091000) |= (0x1);

			ccprintf("Set BAT_LED_BLUE to ouput high.\n");
		} else {
			ccprintf("Second input parameter is not [0|1]\n");
			return EC_ERROR_INVAL;
		}
	} else if (!strcasecmp(argv[1], "1")) {
		/* Check if GPIO direction of BAT_LED_AMBER is output. */
		dir = REG8(0x40099002) & (0x10);
		if (dir)
			ccprintf("GPIO get direction of BAT_LED_AMBER is output\n");
		else {
			ccprintf("GPIO get direction of BAT_LED_AMBER is input\n");
			return EC_ERROR_INVAL;
		}
		if (!strcasecmp(argv[2], "0")) {
			/*
			 * Set BAT_LED_AMBER to output low.
			 * BAT_LED_AMBER is PIN(C,4) => port = 0xC, mask = 0x10
			 * 0x40081000 (gpio base address) + 0xC (port) * 0x2000L (offset) + 0x00 (data out) = 0x40099000
			 */
			REG8(0x40099000) &= ~(0x10);

			ccprintf("Set BAT_LED_AMBER to ouput low.\n");
		} else if (!strcasecmp(argv[2], "1")) {
			/*
			 * Set BAT_LED_AMBER to output high.
			 */
			REG8(0x40099000) |= (0x10);

			ccprintf("Set BAT_LED_AMBER to ouput high.\n");
		} else {
			ccprintf("Second input parameter is not [0|1]\n");
			return EC_ERROR_INVAL;
		}

	} else {
		ccprintf("First input parameter is not [0|1]\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(set_out_level, gpio_set_output_level_func,
		"[0|1] [0|1]",
		"GPIO set batt LED output level");

/* Get battery led gpio output type. */
static int gpio_get_output_type_func(int argc, char **argv)
{
	int dir = 0;
	int type = 0;
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

/*
 * Usage:
 * 	get_out_type [0|1]
 *
 * 	0: Get gpio output type of BAT_LED_BLUE
 * 	1: Get gpio output type of BAT_LED_AMBER
 */

	if (!strcasecmp(argv[1], "0")) {
		/* Check if GPIO direction of BAT_LED_BLUE is output. */
		dir = REG8(0x40091002) & (0x1);
		if (dir) {
			ccprintf("GPIO get direction of BAT_LED_BLUE is output, ");
			/*
			 * BAT_LED_BLUE is PIN(8,0) => port = 0x8, mask = 0x1
			 * 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x06 (out type) = 0x40091006
			 */
			type = REG8(0x40091006) & (0x1);
			if (type)
				ccprintf("type is Open-drain.\n");
			else
				ccprintf("type is Push-pull.\n");
		}
		else {
			ccprintf("GPIO get direction of BAT_LED_BLUE is input\n");
			return EC_ERROR_INVAL;
		}
	} else if (!strcasecmp(argv[1], "1")) {
		/* Check if GPIO direction of BAT_LED_AMBER is output. */
		dir = REG8(0x40099002) & (0x10);
		if (dir) {
			ccprintf("GPIO get direction of BAT_LED_AMBER is output, ");
			/*
			 * BAT_LED_AMBER is PIN(C,4) => port = 0xC, mask = 0x10
			 * 0x40081000 (gpio base address) + 0xC (port) * 0x2000L (offset) + 0x06 (out type) = 0x40099006
			 */
			type = REG8(0x40099006) & (0x10);
			if (type)
				ccprintf("type is Open-drain.\n");
			else
				ccprintf("type is Push-pull.\n");
		}
		else {
			ccprintf("GPIO get direction of BAT_LED_AMBER is input\n");
			return EC_ERROR_INVAL;
		}
	} else {
		ccprintf("Input parameter is not [0|1]\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;

}
DECLARE_CONSOLE_COMMAND(get_out_type, gpio_get_output_type_func,
		"[0|1]",
		"GPIO get batt LED output type");

/* Set battery led gpio output type. */
static int gpio_set_output_type_func(int argc, char **argv)
{
	int dir = 0;
	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

/*
 * Usage:
 * 	set_out_type [0|1] [0|1]
 *
 * First parameter:
 * 	0: BAT_LED_BLUE
 * 	1: BAT_LED_AMBER
 *
 * Second parameter:
 * 	0: set gpio output type to Push-pull.
 * 	1: set gpio output type to Open-drain.
 */

	if (!strcasecmp(argv[1], "0")) {
		/* Check if GPIO direction of BAT_LED_BLUE is output. */
		dir = REG8(0x40091002) & (0x1);
		if (dir)
			ccprintf("GPIO get direction of BAT_LED_BLUE is output\n");
		else {
			ccprintf("GPIO get direction of BAT_LED_BLUE is input\n");
			return EC_ERROR_INVAL;
		}
		if (!strcasecmp(argv[2], "0")) {
			/*
			 * Set BAT_LED_BLUE output type to Push-pull.
			 * BAT_LED_BLUE is PIN(8,0) => port = 0x8, mask = 0x1
			 * 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x06 (out type) = 0x40091006
			 */
			REG8(0x40091006) &= ~(0x1);

			ccprintf("Set BAT_LED_BLUE ouput type to Push-pull.\n");
		} else if (!strcasecmp(argv[2], "1")) {
			/*
			 * Set BAT_LED_BLUE ouput type to Open-drain.
			 */
			REG8(0x40091006) |= (0x1);

			ccprintf("Set BAT_LED_BLUE ouput type to Open-drain.\n");
		} else {
			ccprintf("Second input parameter is not [0|1]\n");
			return EC_ERROR_INVAL;
		}
	} else if (!strcasecmp(argv[1], "1")) {
		/* Check if GPIO direction of BAT_LED_AMBER is output. */
		dir = REG8(0x40099002) & (0x10);
		if (dir)
			ccprintf("GPIO get direction of BAT_LED_AMBER is output\n");
		else {
			ccprintf("GPIO get direction of BAT_LED_AMBER is input\n");
			return EC_ERROR_INVAL;
		}
		if (!strcasecmp(argv[2], "0")) {
			/*
			 * Set BAT_LED_AMBER ouput type to Push-pull.
			 * BAT_LED_AMBER is PIN(C,4) => port = 0xC, mask = 0x10
			 * 0x40081000 (gpio base address) + 0xC (port) * 0x2000L (offset) + 0x06 (out type) = 0x40099006
			 */
			REG8(0x40099006) &= ~(0x10);

			ccprintf("Set BAT_LED_AMBER ouput type to Push-pull.\n");
		} else if (!strcasecmp(argv[2], "1")) {
			/*
			 * Set BAT_LED_AMBER ouput type to Open-drain.
			 */
			REG8(0x40099006) |= (0x10);

			ccprintf("Set BAT_LED_AMBER ouput type to Open-drain.\n");
		} else {
			ccprintf("Second input parameter is not [0|1]\n");
			return EC_ERROR_INVAL;
		}

	} else {
		ccprintf("First input parameter is not [0|1]\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(set_out_type, gpio_set_output_type_func,
		"[0|1] [0|1]",
		"GPIO set batt LED output type");

/* Get volume up button gpio direction and input level. */
static int gpio_get_vol_up_func(int argc, char **argv)
{
	int dir = 0;
	int level = 0;
	if (argc != 1)
		return EC_ERROR_PARAM_COUNT;
/*
 * Usage:
 * 	gpio_vol_up
 */

	/*
	* EC_VOLUP_BTN_ODL is PIN(8,3) => port = 0x8, mask = 0x8
	* 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x02 (direction) = 0x40091002
	*/
	dir = REG8(0x40091002) & (0x8);

	if (dir) {
		ccprintf("GPIO get direction of EC_VOLUP_BTN_ODL is output, ERROR!\n");
		return EC_ERROR_INVAL;
	} else {
		ccprintf("GPIO get direction of EC_VOLUP_BTN_ODL is input, valid!\n");
		/*
		* EC_VOLUP_BTN_ODL is PIN(8,3) => port = 0x8, mask = 0x8
		* 0x40081000 (gpio base address) + 0x8 (port) * 0x2000L (offset) + 0x01 (data in) = 0x40091001
		*/
		level = REG8(0x40091001) & (0x8);
		if (level)
			ccprintf("GPIO get input level of EC_VOLUP_BTN_ODL is high.\n");
		else
			ccprintf("GPIO get input level of EC_VOLUP_BTN_ODL is low.\n");
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(gpio_vol_up, gpio_get_vol_up_func, NULL,
		"GPIO get direction and input level of EC_VOLUP_BTN_ODL");
