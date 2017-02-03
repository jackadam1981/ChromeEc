/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <endian.h>

#include "clock.h"
#include "common.h"
#include "console.h"
#include "dcrypto/dcrypto.h"
#include "device_state.h"
#include "ec_version.h"
#include "extension.h"
#include "flash.h"
#include "flash_config.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "init_chip.h"
#include "nvmem.h"
#include "rdd.h"
#include "registers.h"
#include "scratch_reg1.h"
#include "signed_header.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "uartn.h"
#include "usb_descriptor.h"
#include "usb_hid.h"
#include "usb_spi.h"
#include "usb_i2c.h"
#include "util.h"

/* Define interrupt and gpio structs */
#include "gpio_list.h"

#include "cryptoc/sha.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

#define NVMEM_CR50_SIZE 272
#define NVMEM_TPM_SIZE ((sizeof((struct nvmem_partition *)0)->buffer) \
			- NVMEM_CR50_SIZE)

/* NvMem user buffer lengths table */
uint32_t nvmem_user_sizes[NVMEM_NUM_USERS] = {
	NVMEM_CR50_SIZE
};

/*  Board specific configuration settings */
static uint32_t board_properties;
static uint8_t reboot_request_posted;

int board_has_ap_usb(void)
{
	return !!(board_properties & BOARD_USB_AP);
}

int board_use_plt_rst(void)
{
	return !!(board_properties & BOARD_USE_PLT_RESET);
}

int board_rst_pullup_needed(void)
{
	return !!(board_properties & BOARD_NEEDS_SYS_RST_PULL_UP);
}

/* I2C Port definition */
const struct i2c_port_t i2c_ports[]  = {
	{"master", I2C_PORT_MASTER, 100,
	 GPIO_I2C_SCL_INA, GPIO_I2C_SDA_INA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Strapping pin info structure */
#define STRAP_PIN_DELAY_USEC  100
enum strap_list {
	a0,
	a1,
	b0,
	b1,
};

struct strap_desc {
	/* GPIO enum from gpio.inc for the strap pin */
	uint8_t gpio_signal;
	/* Offset into pinmux register section for pad SEL register */
	uint8_t sel_offset;
	/* Entry in the pinmux peripheral selector table for pad */
	uint8_t pad_select;
};

struct board_cfg {
	/* Value the strap pins should read for a given board */
	uint8_t strap_cfg;
	/* Properties required for a given board */
	uint32_t board_properties;
};

/*
 * This table contains both the GPIO and pad specific information required to
 * configure each strapping pin to be either a GPIO input or output.
 */
const struct strap_desc strap_regs[] = {
	{ GPIO_STRAP_A0, GOFFSET(PINMUX, DIOA1_SEL), GC_PINMUX_DIOA1_SEL, },
	{ GPIO_STRAP_A1, GOFFSET(PINMUX, DIOA9_SEL), GC_PINMUX_DIOA9_SEL, },
	{ GPIO_STRAP_B0, GOFFSET(PINMUX, DIOA6_SEL), GC_PINMUX_DIOA6_SEL, },
	{ GPIO_STRAP_B1, GOFFSET(PINMUX, DIOA12_SEL), GC_PINMUX_DIOA12_SEL, },
};

void post_reboot_request(void)
{
	/* Reboot the device next time TPM reset is requested. */
	reboot_request_posted = 1;
}

/*****************************************************************************/
/*                                                                           */

/*
 * Battery cutoff monitor is needed on the devices where hardware alone does
 * not provide proper battery cutoff functionality.
 *
 * The sequence is as follows: set up an interrupt to react to the charger
 * disconnect event. When the interrupt happens observe status of the buttons
 * connected to PWRB_IN and KEY0_IN.
 *
 * If both are pressed, start the 5 second timeout, while keeping monitoring
 * the charger connection state. If it remains disconnected for the entire
 * duration - generate 5 second pulses on EC_RST_L and BAT_EN outputs.
 *
 * In reality the BAT_EN output pulse will cause the complete power cut off,
 * so strictly speaking the code does not need to do anything once BAT_EN
 * output is deasserted.
 */

/* Time to wait before initiating battery cutoff procedure. */
#define CUTOFF_TIMEOUT_US (5 * SECOND)

/* A timeout hook to run in the end of the 5 s interval. */
static void ac_stayed_disconnected(void)
{
	uint32_t saved_override_state;

	CPRINTS("%s", __func__);

	/* assert EC_RST_L and deassert BAT_EN */
	GREG32(RBOX, ASSERT_EC_RST) = 1;

	/*
	 * BAT_EN needs to use the RBOX override ability, bit 1 is battery
	 * disable bit.
	 */
	saved_override_state = GREG32(RBOX, OVERRIDE_OUTPUT);
	GWRITE_FIELD(RBOX, OVERRIDE_OUTPUT, VAL, 0); /* Setting it to zero. */
	GWRITE_FIELD(RBOX, OVERRIDE_OUTPUT, OEN, 1);
	GWRITE_FIELD(RBOX, OVERRIDE_OUTPUT, EN, 1);


	msleep(5000);

	/*
	 * The system was supposed to be shut down the moment battery
	 * disconnect was asserted, but if we made it here we might as well
	 * restore the original state.
	 */
	GREG32(RBOX, OVERRIDE_OUTPUT) = saved_override_state;
	GREG32(RBOX, ASSERT_EC_RST) = 0;
}
DECLARE_DEFERRED(ac_stayed_disconnected);

/*
 * Just a shortcut to make use of these AC power interrupt states better
 * readable. RED means rising edge and FED means falling edge.
 */
enum {
	ac_pres_red = GC_RBOX_INT_STATE_INTR_AC_PRESENT_RED_MASK,
	ac_pres_fed = GC_RBOX_INT_STATE_INTR_AC_PRESENT_FED_MASK,
	buttons_not_pressed = GC_RBOX_CHECK_INPUT_KEY0_IN_MASK |
		GC_RBOX_CHECK_INPUT_PWRB_IN_MASK
};

/*
 * ISR reacting to both falling and raising edges of the AC_PRESENT signal.
 * Falling edge indicates pulling out of the charger cable and vice versa.
 */
static void ac_power_state_changed(void)
{
	uint32_t req;

	/* Get current status and clear it. */
	req = GREG32(RBOX, INT_STATE) & (ac_pres_red | ac_pres_fed);
	GREG32(RBOX, INT_STATE) = req;

	CPRINTS("%s: status 0x%x", __func__, req);

	/* Raising edge gets priority, stop timeout timer and go. */
	if (req & ac_pres_red) {
		hook_call_deferred(&ac_stayed_disconnected_data, -1);
		return;
	}

	/*
	 * If this is not a falling edge, or either of the buttons is not
	 * pressed - bail out.
	 */
	if (!(req & ac_pres_fed) ||
	    (GREG32(RBOX, CHECK_INPUT) & buttons_not_pressed))
		return;

	/*
	 * Charger cable was yanked while the power and key0 buttons were kept
	 * pressed - user wants a battery cut off.
	 */
	hook_call_deferred(&ac_stayed_disconnected_data, CUTOFF_TIMEOUT_US);
}
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_AC_PRESENT_RED_INT, ac_power_state_changed, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_AC_PRESENT_FED_INT, ac_power_state_changed, 1);

/* Enable interrupts on plugging in and yanking out of the charger cable. */
static void set_up_battery_cutoff_monitor(void)
{
	/* It is set in idle.c also. */
	GWRITE_FIELD(RBOX, WAKEUP, ENABLE, 1);

	GWRITE_FIELD(RBOX, INT_ENABLE, INTR_AC_PRESENT_RED, 1);
	GWRITE_FIELD(RBOX, INT_ENABLE, INTR_AC_PRESENT_FED, 1);

	task_enable_irq(GC_IRQNUM_RBOX0_INTR_AC_PRESENT_RED_INT);
	task_enable_irq(GC_IRQNUM_RBOX0_INTR_AC_PRESENT_FED_INT);
}
/*                                                                           */
/*****************************************************************************/

/*
 * There's no way to trigger on both rising and falling edges, so force a
 * compiler error if we try. The workaround is to use the pinmux to connect
 * two GPIOs to the same input and configure each one for a separate edge.
 */
#define GPIO_INT(name, pin, flags, signal)	\
	BUILD_ASSERT(((flags) & GPIO_INT_BOTH) != GPIO_INT_BOTH);
#include "gpio.wrap"

static void init_pmu(void)
{
	clock_enable_module(MODULE_PMU, 1);

	/* This boot sequence may be a result of previous soft reset,
	 * in which case the PMU low power sequence register needs to
	 * be reset.
	 */
	GREG32(PMU, LOW_POWER_DIS) = 0;

	/* Enable wakeup interrupt */
	task_enable_irq(GC_IRQNUM_PMU_INTR_WAKEUP_INT);
	GWRITE_FIELD(PMU, INT_ENABLE, INTR_WAKEUP, 1);
}

static void init_interrupts(void)
{
	int i;
	uint32_t exiten = GREG32(PINMUX, EXITEN0);

	/* Clear wake pin interrupts */
	GREG32(PINMUX, EXITEN0) = 0;
	GREG32(PINMUX, EXITEN0) = exiten;

	/* Enable all GPIO interrupts */
	for (i = 0; i < gpio_ih_count; i++)
		if (gpio_list[i].flags & GPIO_INT_ANY)
			gpio_enable_interrupt(i);
}

static void configure_board_specific_gpios(void)
{
	/* Add a pullup to sys_rst_l */
	if (board_rst_pullup_needed())
		GWRITE_FIELD(PINMUX, DIOM0_CTL, PU, 1);

	/*
	 * Connect either plt_rst_l or sys_rst_l to GPIO_TPM_RST_L based on the
	 * board type. This signal is used to monitor AP resets and reset the
	 * TPM.
	 *
	 * plt_rst_l is on diom3, and sys_rst_l is on diom0.
	 */
	if (board_use_plt_rst()) {
		/* Use plt_rst_l as the tpm reset signal. */
		GWRITE(PINMUX, GPIO1_GPIO0_SEL, GC_PINMUX_DIOM3_SEL);
		/* Enbale the input */
		GWRITE_FIELD(PINMUX, DIOM3_CTL, IE, 1);

		/* Set power down for the equivalent of DIO_WAKE_FALLING */
		/* Set to be edge sensitive */
		GWRITE_FIELD(PINMUX, EXITEDGE0, DIOM3, 1);
		/* Select failling edge polarity */
		GWRITE_FIELD(PINMUX, EXITINV0, DIOM3, 1);
		/* Enable powerdown exit on DIOM3 */
		GWRITE_FIELD(PINMUX, EXITEN0, DIOM3, 1);
	} else {
		/* Use sys_rst_l as the tpm reset signal. */
		GWRITE(PINMUX, GPIO1_GPIO0_SEL, GC_PINMUX_DIOM0_SEL);
		/* Enbale the input */
		GWRITE_FIELD(PINMUX, DIOM0_CTL, IE, 1);

		/* Set power down for the equivalent of DIO_WAKE_FALLING */
		/* Set to be edge sensitive */
		GWRITE_FIELD(PINMUX, EXITEDGE0, DIOM0, 1);
		/* Select failling edge polarity */
		GWRITE_FIELD(PINMUX, EXITINV0, DIOM0, 1);
		/* Enable powerdown exit on DIOM0 */
		GWRITE_FIELD(PINMUX, EXITEN0, DIOM0, 1);
	}
}

void decrement_retry_counter(void)
{
	uint32_t counter = GREG32(PMU, LONG_LIFE_SCRATCH0);

	if (counter) {
		GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG0, 1);
		GREG32(PMU, LONG_LIFE_SCRATCH0) = counter - 1;
		GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG0, 0);
	}
}

/* Initialize board. */
static void board_init(void)
{
	/*
	 * Deep sleep resets should be considered valid and should not impact
	 * the rolling reboot count.
	 */
	if (system_get_reset_flags() & RESET_FLAG_HIBERNATE)
		decrement_retry_counter();
	configure_board_specific_gpios();
	init_pmu();
	init_interrupts();
	init_trng();
	init_jittery_clock(1);
	init_runlevel(PERMISSION_MEDIUM);
	/* Initialize NvMem partitions */
	nvmem_init();

	/* Indication that firmware is running, for debug purposes. */
	GREG32(PMU, PWRDN_SCRATCH16) = 0xCAFECAFE;

	/* Enable battery cutoff software support on detachable devices. */
	if (system_battery_cutoff_support_required())
		set_up_battery_cutoff_monitor();

	ccd_force_enable();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

#if defined(CONFIG_USB)
const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Cr50"),
	[USB_STR_VERSION] = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Shell"),
	[USB_STR_BLOB_NAME] = USB_STRING_DESC("Blob"),
	[USB_STR_HID_KEYBOARD_NAME] = USB_STRING_DESC("PokeyPokey"),
	[USB_STR_AP_NAME] = USB_STRING_DESC("AP"),
	[USB_STR_EC_NAME] = USB_STRING_DESC("EC"),
	[USB_STR_UPGRADE_NAME] = USB_STRING_DESC("Firmware upgrade"),
	[USB_STR_SPI_NAME] = USB_STRING_DESC("AP EC upgrade"),
	[USB_STR_SERIALNO] = USB_STRING_DESC(DEFAULT_SERIALNO),
	[USB_STR_I2C_NAME] = USB_STRING_DESC("I2C"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);
#endif

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	[CONFIG_SPI_FLASH_PORT] = {0, 2, GPIO_COUNT}
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

int flash_regions_to_enable(struct g_flash_region *regions,
			    int max_regions)
{
	/*
	 * This needs to account for two regions: the "other" RW partition and
	 * the NVRAM in TOP_B.
	 *
	 * When running from RW_A the two regions are adjacent, but it is
	 * simpler to keep function logic the same and always configure two
	 * separate regions.
	 */

	if (max_regions < 3)
		return 0;

	/* Enable access to the other RW image... */
	if (system_get_image_copy() == SYSTEM_IMAGE_RW)
		/* Running RW_A, enable RW_B */
		regions[0].reg_base = CONFIG_MAPPED_STORAGE_BASE +
			CONFIG_RW_B_MEM_OFF;
	else
		/* Running RW_B, enable RW_A */
		regions[0].reg_base = CONFIG_MAPPED_STORAGE_BASE +
			CONFIG_RW_MEM_OFF;
	/* Size is the same */
	regions[0].reg_size = CONFIG_RW_SIZE;
	regions[0].reg_perms = FLASH_REGION_EN_ALL;

	/* Enable access to the NVRAM partition A region */
	regions[1].reg_base = CONFIG_MAPPED_STORAGE_BASE +
		CONFIG_FLASH_NVMEM_OFFSET_A;
	regions[1].reg_size = NVMEM_PARTITION_SIZE;
	regions[1].reg_perms = FLASH_REGION_EN_ALL;

	/* Enable access to the NVRAM partition B region */
	regions[2].reg_base = CONFIG_MAPPED_STORAGE_BASE +
		CONFIG_FLASH_NVMEM_OFFSET_B;
	regions[2].reg_size = NVMEM_PARTITION_SIZE;
	regions[2].reg_perms = FLASH_REGION_EN_ALL;

	return 3;
}

static void enable_uart(int uart)
{
	/* Enable RX and TX on the UART peripheral */
	uartn_enable(uart);

	/* Connect the TX pin to the UART TX Signal */
	if (!uartn_enabled(uart))
		uartn_tx_connect(uart);
}

static void disable_uart(int uart)
{
	/* Disable RX and TX on the UART peripheral */
	uartn_disable(uart);

	/* Disconnect the TX pin from the UART peripheral */
	uartn_tx_disconnect(uart);
}

static void enable_ap_uart(void)
{
	enable_uart(UART_AP);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, enable_ap_uart, HOOK_PRIO_DEFAULT);

static void disable_ap_uart(void)
{
	disable_uart(UART_AP);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, disable_ap_uart, HOOK_PRIO_DEFAULT);

void device_state_on(enum gpio_signal signal)
{
	gpio_disable_interrupt(signal);

	switch (signal) {
	default:
		CPRINTS("Device not supported");
		return;
	}
}

static void init_board_properties(void)
{
	uint32_t properties;

	properties = GREG32(PMU, LONG_LIFE_SCRATCH1);

	/*
	 * This must be a power on reset or maybe restart due to a software
	 * update from a version not setting the register.
	 */
	if (!(properties & BOARD_ALL_PROPERTIES) || (system_get_reset_flags() &
						     RESET_FLAG_HARD)) {
		/*
		 * Mask board properties because following hard reset, they
		 * won't be cleared.
		 */
		properties &= ~BOARD_ALL_PROPERTIES;
		/* properties |= get_properties(); */
		/*
		 * Now save the properties value for future use.
		 *
		 * Enable access to LONG_LIFE_SCRATCH1 reg.
		 */
		GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 1);
		/* Save properties in LONG_LIFE register */
		GREG32(PMU, LONG_LIFE_SCRATCH1) = properties;
		/* Disable access to LONG_LIFE_SCRATCH1 reg */
		GWRITE_FIELD(PMU, LONG_LIFE_SCRATCH_WR_EN, REG1, 0);
	}
	/* Save this configuration setting */
	board_properties = properties;
}
DECLARE_HOOK(HOOK_INIT, init_board_properties, HOOK_PRIO_FIRST);

/* Determine key type based on the key ID. */
static const char *key_type(uint32_t key_id)
{

	/*
	 * It is a mere convention, but all prod keys are required to have key
	 * IDs such, that bit D2 is set, and all dev keys are required to have
	 * key IDs such, that bit D2 is not set.
	 *
	 * This convention is enforced at the key generation time.
	 */
	if (key_id & (1 << 2))
		return "prod";
	else
		return "dev";
}

static int command_sysinfo(int argc, char **argv)
{
	enum system_image_copy_t active;
	uintptr_t vaddr;
	const struct SignedHeader *h;

	ccprintf("Reset flags: 0x%08x (", system_get_reset_flags());
	system_print_reset_flags();
	ccprintf(")\n");

	ccprintf("Chip:        %s %s %s\n", system_get_chip_vendor(),
		 system_get_chip_name(), system_get_chip_revision());

	active = system_get_ro_image_copy();
	vaddr = get_program_memory_addr(active);
	h = (const struct SignedHeader *)vaddr;
	ccprintf("RO keyid:    0x%08x(%s)\n", h->keyid, key_type(h->keyid));

	active = system_get_image_copy();
	vaddr = get_program_memory_addr(active);
	h = (const struct SignedHeader *)vaddr;
	ccprintf("RW keyid:    0x%08x(%s)\n", h->keyid, key_type(h->keyid));

	ccprintf("DEV_ID:      0x%08x 0x%08x\n",
		 GREG32(FUSE, DEV_ID0), GREG32(FUSE, DEV_ID1));

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(sysinfo, command_sysinfo,
			     NULL,
			     "Print system info");

/*
 * SysInfo command:
 * There are no input args.
 * Output is this struct, all fields in network order.
 */
struct sysinfo_s {
	uint32_t ro_keyid;
	uint32_t rw_keyid;
	uint32_t dev_id0;
	uint32_t dev_id1;
} __packed;

static int command_board_properties(int argc, char **argv)
{
	ccprintf("properties = 0x%x\n", board_properties);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(brdprop, command_board_properties,
			     NULL, "Display board properties");
