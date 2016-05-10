/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "device_state.h"
#include "ec_version.h"
#include "flash_config.h"
#include "gpio.h"
#include "hooks.h"
#include "init_chip.h"
#include "registers.h"
#include "task.h"
#include "trng.h"
#include "uartn.h"
#include "usb_descriptor.h"
#include "usb_hid.h"
#include "util.h"
#include "spi.h"

/* Define interrupt and gpio structs */
#include "gpio_list.h"

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
	/* This boot sequence may be a result of previous soft reset,
	 * in which case the PMU low power sequence register needs to
	 * be reset. */
	GREG32(PMU, LOW_POWER_DIS) = 0;
}

static void init_timers(void)
{
	/* Cancel low speed timers that may have
	 * been initialized prior to soft reset. */
	GREG32(TIMELS, TIMER0_CONTROL) = 0;
	GREG32(TIMELS, TIMER0_LOAD) = 0;
	GREG32(TIMELS, TIMER1_CONTROL) = 0;
	GREG32(TIMELS, TIMER1_LOAD) = 0;
}

static void init_interrupts(void)
{
	int i;

	/* Enable all GPIO interrupts */
	for (i = 0; i < gpio_ih_count; i++)
		if (gpio_list[i].flags & GPIO_INT_ANY)
			gpio_enable_interrupt(i);
}

enum permission_level {
	PERMISSION_LOW = 0x00,
	PERMISSION_MEDIUM = 0x33,    /* APPS run at medium */
	PERMISSION_HIGH = 0x3C,
	PERMISSION_HIGHEST = 0x55
};

/* Drop run level to at least medium. */
static void init_runlevel(const enum permission_level desired_level)
{
	volatile uint32_t *const reg_addrs[] = {
		/* CPU's use of the system peripheral bus */
		GREG32_ADDR(GLOBALSEC, CPU0_S_PERMISSION),
		/* CPU's use of the system bus via the debug access port */
		GREG32_ADDR(GLOBALSEC, CPU0_S_DAP_PERMISSION),
		/* DMA's use of the system peripheral bus */
		GREG32_ADDR(GLOBALSEC, DDMA0_PERMISSION),
		/* Current software level affects which (if any) scratch
		 * registers can be used for a warm boot hardware-verified
		 * jump. */
		GREG32_ADDR(GLOBALSEC, SOFTWARE_LVL),
	};
	int i;

	/* Permission registers drop by 1 level (e.g. HIGHEST -> HIGH)
	 * each time a write is encountered (the value written does
	 * not matter).  So we repeat writes and reads, until the
	 * desired level is reached.
	 */
	for (i = 0; i < ARRAY_SIZE(reg_addrs); i++) {
		uint32_t current_level;

		while (1) {
			current_level = *reg_addrs[i];
			if (current_level <= desired_level)
				break;
			*reg_addrs[i] = desired_level;
		}
	}
}

/* Initialize board. */
static void board_init(void)
{
	init_pmu();
	init_timers();
	init_interrupts();
	init_trng();
	init_jittery_clock(1);			/* high-security mode */
	init_runlevel(PERMISSION_MEDIUM);

	/* TODO(crosbug.com/p/49959): For now, leave flash WP unlocked */
	GREG32(RBOX, EC_WP_L) = 1;

	/* Indication that firmware is running, for debug purposes. */
	GREG32(PMU, PWRDN_SCRATCH16) = 0xCAFECAFE;
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
	[USB_STR_HID_NAME] = USB_STRING_DESC("PokeyPokey"),
	[USB_STR_AP_NAME] = USB_STRING_DESC("AP"),
	[USB_STR_EC_NAME] = USB_STRING_DESC("EC"),
	[USB_STR_UPGRADE_NAME] = USB_STRING_DESC("Firmware upgrade"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);
#endif

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	[CONFIG_SPI_FLASH_PORT] = {0, 4, GPIO_COUNT}
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

int flash_regions_to_enable(struct g_flash_region *regions,
			    int max_regions)
{
	uint32_t half = CONFIG_FLASH_SIZE / 2;

	if (max_regions < 1)
		return 0;

	if ((uint32_t)flash_regions_to_enable <
	    (CONFIG_MAPPED_STORAGE_BASE + half))
		/*
		 * Running from RW_A. Need to enable writes into the top half,
		 * which consists of NV_RAM and RW_B sections.
		 */
		regions->reg_base = CONFIG_MAPPED_STORAGE_BASE + half;
	else
		/*
		 * Running from RW_B, need to enable access to both program
		 * memory in the lower half and the NVRAM space in the top
		 * half.
		 *
		 * NVRAM space in the top half by design is at the same offset
		 * and of the same size as the RO section in the lower half.
		 */
		regions->reg_base = CONFIG_MAPPED_STORAGE_BASE +
			CONFIG_RO_SIZE;

	/* The size of the write enable area is the same in both cases. */
	regions->reg_size = half;
	regions->reg_perms =  FLASH_REGION_EN_ALL;

	return 1; /* One region is enough. */
}

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

void sys_rst_asserted(enum gpio_signal signal)
{
	/* TODO(crosbug.com/p/52366): Do something useful here. */
	CPRINTS("%s(%d)", __func__, signal);
}

/*
 * If both UARTs are enabled we cant tell anything about the
 * state of servo, so disable servo detection.
 */
static int servo_state_unknown(void)
{
	if (uartn_enabled(UART_AP) && uartn_enabled(UART_EC)) {
		device_set_state(DEVICE_SERVO_EC, DEVICE_STATE_UNKNOWN);
		device_set_state(DEVICE_SERVO_AP, DEVICE_STATE_UNKNOWN);
		return 1;
	}
	return 0;
}

static void servo_deferred(void)
{
	if (servo_state_unknown() ||
	    (device_get_state(DEVICE_SERVO_AP) == DEVICE_STATE_ON) ||
	    (device_get_state(DEVICE_SERVO_EC) == DEVICE_STATE_ON))
		return;
	device_set_state(DEVICE_SERVO_AP, DEVICE_STATE_OFF);
	device_set_state(DEVICE_SERVO_EC, DEVICE_STATE_OFF);
}
DECLARE_DEFERRED(servo_deferred);

static void ap_deferred(void)
{
	if (device_get_state(DEVICE_AP) == DEVICE_STATE_ON)
		return;
	device_set_state(DEVICE_AP, DEVICE_STATE_OFF);
}
DECLARE_DEFERRED(ap_deferred);

static void ec_deferred(void)
{
	if (device_get_state(DEVICE_EC) == DEVICE_STATE_ON)
		return;
	device_set_state(DEVICE_EC, DEVICE_STATE_OFF);
}
DECLARE_DEFERRED(ec_deferred);

struct device_config device_states[] = {
	[DEVICE_SERVO_AP] = {
		.deferred = &servo_deferred_data,
		.detect_on = GPIO_SERVO_UART1_ON,
		.detect_off = GPIO_SERVO_UART1_OFF
	},
	[DEVICE_SERVO_EC] = {
		.deferred = &servo_deferred_data,
		.detect_on = GPIO_SERVO_UART2_ON,
		.detect_off = GPIO_SERVO_UART2_OFF
	},
	[DEVICE_AP] = {
		.deferred = &ap_deferred_data,
		.detect_on = GPIO_AP_ON,
		.detect_off = GPIO_AP_OFF
	},
	[DEVICE_EC] = {
		.deferred = &ec_deferred_data,
		.detect_on = GPIO_EC_ON,
		.detect_off = GPIO_EC_OFF
	},
};
BUILD_ASSERT(ARRAY_SIZE(device_states) == DEVICE_COUNT);

static void device_powered_on(enum device_type device)
{
	device_set_state(device, DEVICE_STATE_ON);
}

void device_state_on(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_AP_ON:
		device_powered_on(DEVICE_AP);
		break;
	case GPIO_EC_ON:
		device_powered_on(DEVICE_EC);
		break;
	default:
		if (servo_state_unknown())
			return;
		device_powered_on(DEVICE_SERVO_EC);
		device_powered_on(DEVICE_SERVO_AP);
	}
}

void device_state_off(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_AP_OFF:
		board_update_device_state(DEVICE_AP);
		break;
	case GPIO_EC_OFF:
		board_update_device_state(DEVICE_EC);
		break;
	default:
		board_update_device_state(DEVICE_SERVO_EC);
		board_update_device_state(DEVICE_SERVO_AP);
	}
}

void board_update_device_state(enum device_type device)
{
	int state;

	if (device == DEVICE_SERVO_EC || device == DEVICE_SERVO_AP) {
		/*
		 * If either AP UART TX or EC UART TX are pulled high when
		 * cr50 uart is not enabled, then servo is attached
		 */
		state = (!uartn_enabled(UART_AP) &&
			gpio_get_level(GPIO_SERVO_UART1_ON)) ||
			(!uartn_enabled(UART_EC) &&
			gpio_get_level(GPIO_SERVO_UART2_ON));
	} else
		state = gpio_get_level(device_states[device].detect_on);

	/*
	 * If the device is currently on set its state immediately. If it
	 * thinks the device is powered off debounce the signal.
	 */
	if (state)
		device_powered_on(device);
	else {
		device_set_state(device, DEVICE_STATE_UNKNOWN);

		/*
		 * Wait a bit. If cr50 detects this device is ever powered on
		 * during this time then the status wont be set to powered off.
		 */
		hook_call_deferred(device_states[device].deferred, 50);
	}
}

static int command_devices(int argc, char **argv)
{
	int state = device_get_state(DEVICE_AP);

	ccprintf("AP %s\n",
		state == DEVICE_STATE_ON ? "on" :
		state == DEVICE_STATE_OFF ? "off" : "unknown");
	state = device_get_state(DEVICE_SERVO_AP);
	ccprintf("SERVO_AP %s\n",
		state == DEVICE_STATE_ON ? "on" :
		state == DEVICE_STATE_OFF ? "off" : "unknown");
	state = device_get_state(DEVICE_SERVO_EC);
	ccprintf("SERVO_EC %s\n",
		state == DEVICE_STATE_ON ? "on" :
		state == DEVICE_STATE_OFF ? "off" : "unknown");
	state = device_get_state(DEVICE_EC);
	ccprintf("EC %s\n",
		state == DEVICE_STATE_ON ? "on" :
		state == DEVICE_STATE_OFF ? "off" : "unknown");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(devices, command_devices,
	"",
	"Get the AP, EC, and servo device states",
	NULL);
