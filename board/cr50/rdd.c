/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "case_closed_debug.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "rbox.h"
#include "rdd.h"
#include "registers.h"
#include "system.h"
#include "uart_bitbang.h"
#include "uartn.h"
#include "usb_api.h"
#include "usb_i2c.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

static int keep_ccd_enabled;
static int enable_usb_wakeup;

struct uart_config {
	const char *name;
	int (*device_is_connected)(void);
	int tx_signal;
};

static struct uart_config uarts[] = {
	[UART_AP] = {"AP", ap_is_connected, GC_PINMUX_UART1_TX_SEL},
	[UART_EC] = {"EC", ec_is_connected, GC_PINMUX_UART2_TX_SEL},
};

static int ccd_is_enabled(void)
{
	return ccd_get_mode() == CCD_MODE_ENABLED;
}

int is_utmi_wakeup_allowed(void)
{
	return enable_usb_wakeup;
}


/* If the UART TX is connected the pinmux select will have a non-zero value */
int uart_tx_is_connected(int uart)
{
	if (uart == UART_AP)
		return GREAD(PINMUX, DIOA7_SEL);
	return GREAD(PINMUX, DIOB5_SEL);
}

/**
 * Connect the UART pin to the given signal
 *
 * @param uart		the uart peripheral number
 * @param signal	the pinmux selector value for the gpio or peripheral
 *			function. 0 to disable the output.
 */
static void uart_select_tx(int uart, int signal)
{
	if (uart == UART_AP) {
		GWRITE(PINMUX, DIOA7_SEL, signal);
	} else {
		GWRITE(PINMUX, DIOB5_SEL, signal);

		/* Remove the pulldown when we are driving the signal */
		GWRITE_FIELD(PINMUX, DIOB5_CTL, PD, signal ? 0 : 1);
	}
}

void uartn_tx_connect(int uart)
{
	if (uart == UART_AP && !ccd_is_cap_enabled(CCD_CAP_AP_RX_CR50_TX))
		return;

	if (uart == UART_EC && !ccd_is_cap_enabled(CCD_CAP_EC_RX_CR50_TX))
		return;

	if (!ccd_is_enabled())
		return;

	if (servo_is_connected()) {
		CPRINTS("Servo attached; cannot enable %s UART",
			uarts[uart].name);
		return;
	}

	if (uarts[uart].device_is_connected())
		uart_select_tx(uart, uarts[uart].tx_signal);
	else if (!uart_tx_is_connected(uart))
		CPRINTS("%s is powered off", uarts[uart].name);
}

void uartn_tx_disconnect(int uart)
{
	/* Disconnect the TX pin from UART peripheral */
	uart_select_tx(uart, 0);
}

static void configure_ccd(int enable)
{
	if (enable) {
		if (ccd_is_enabled())
			return;

		/* Enable CCD */
		ccd_set_mode(CCD_MODE_ENABLED);

		enable_usb_wakeup = 1;
	} else {
		enable_usb_wakeup = board_has_ap_usb();

		/* Disable CCD */
		ccd_set_mode(CCD_MODE_DISABLED);
	}

	rdd_update_state();

	CPRINTS("CCD is now %sabled.", enable ? "en" : "dis");
}

void rdd_attached(void)
{
	/* Change CCD_MODE_L to an output which follows the internal GPIO. */
	GWRITE(PINMUX, DIOM1_SEL, GC_PINMUX_GPIO0_GPIO5_SEL);
	/* Indicate case-closed debug mode (active low) */
	gpio_set_flags(GPIO_CCD_MODE_L, GPIO_OUT_LOW);
}

void rdd_detached(void)
{
	/*
	 * Done with case-closed debug mode, therefore re-setup the CCD_MODE_L
	 * pin as an input only if CCD mode isn't being forced enabled.
	 *
	 * NOTE: A pull up is required on this pin, however it was already
	 * configured during the set up of the pinmux in gpio_pre_init().  The
	 * chip-specific GPIO module will ignore any pull up/down configuration
	 * anyways.
	 */
	if (!keep_ccd_enabled)
		gpio_set_flags(GPIO_CCD_MODE_L, GPIO_INPUT);
}

static void rdd_check_pin(void)
{
	/* The CCD mode pin is active low. */
	int enable = !gpio_get_level(GPIO_CCD_MODE_L);

	/* Keep CCD enabled if it's being forced enabled. */
	if (keep_ccd_enabled)
		enable = 1;

	if (enable == ccd_is_enabled())
		return;

	configure_ccd(enable);
}
DECLARE_HOOK(HOOK_SECOND, rdd_check_pin, HOOK_PRIO_DEFAULT);

/*
 * Flags for the current RDD state.  This is used for determining what
 * state we're in now and what state we should be in.
 */
enum rdd_flag {
	/* Individual flags */
	RDD_FLAG_UART_AP		= (1 << 0),
	RDD_FLAG_UART_AP_TX		= (1 << 1),
	RDD_FLAG_UART_EC		= (1 << 2),
	RDD_FLAG_UART_EC_TX		= (1 << 3),
	RDD_FLAG_UART_EC_BITBANG	= (1 << 4),
	RDD_FLAG_I2C			= (1 << 5),

	/*
	 * TODO: SPI is currently checked on a per-packet basis.  We could
	 * disable the entire SPI endpoint if neither AP nor EC flash access
	 * is allowed, though.
	 */

	/* Combos */
	/* Flags that CCD wants to enable */
	RDD_FLAGS_CCD = (RDD_FLAG_UART_AP | RDD_FLAG_UART_AP_TX |
			 RDD_FLAG_UART_EC | RDD_FLAG_UART_EC_TX |
			 RDD_FLAG_I2C),

	/* Flags that servo wants to disable */
	RDD_FLAGS_SERVO_DISABLE = (RDD_FLAG_UART_AP | RDD_FLAG_UART_AP_TX |
				   RDD_FLAG_UART_EC | RDD_FLAG_UART_EC_TX |
				   RDD_FLAG_UART_EC_BITBANG | RDD_FLAG_I2C),
};

/**
 * Return the currently enabled RDD flags (see enum rdd_flag).
 */
static uint32_t rdd_get_flags(void)
{
	uint32_t flags_now = 0;

	if (uartn_is_enabled(UART_AP))
		flags_now |= RDD_FLAG_UART_AP;
	if (uart_tx_is_connected(UART_AP))
		flags_now |= RDD_FLAG_UART_AP_TX;
	if (uartn_is_enabled(UART_EC))
		flags_now |= RDD_FLAG_UART_EC;
	if (uart_tx_is_connected(UART_EC))
		flags_now |= RDD_FLAG_UART_EC_TX;

#ifdef CONFIG_UART_BITBANG
	if (uart_bitbang_is_enabled(UART_EC))
		flags_now |= RDD_FLAG_UART_EC_BITBANG;
#endif

	if (usb_i2c_board_is_enabled())
		flags_now |= RDD_FLAG_I2C;

	return flags_now;
}

static void rdd_change_hook(void)
{
	uint32_t flags_now;
	uint32_t flags_want = 0;
	uint32_t delta;

	/* Check what's enabled now */
	flags_now = rdd_get_flags();

	/* Start out by figuring what flags we might want enabled */

	/* CCD will try to enable everything, unless otherwise disabled */
	if (ccd_get_mode() == CCD_MODE_ENABLED)
		flags_want |= RDD_FLAGS_CCD;

#ifdef CONFIG_UART_BITBANG
	if (uart_bitbang_is_wanted(UART_EC))
		flags_want |= RDD_FLAG_UART_EC_BITBANG;
#endif

	/* Then disable flags we can't have */

	/* Servo takes over all the UARTs and I2C */
	if (servo_is_connected())
		flags_want &= RDD_FLAGS_SERVO_DISABLE;

	/* Disable UARTs for AP or EC if that device is not on */
	if (!ap_is_connected())
		flags_want &= ~(RDD_FLAG_UART_AP | RDD_FLAG_UART_AP_TX);
	if (!ec_is_connected())
		flags_want &= ~(RDD_FLAG_UART_EC | RDD_FLAG_UART_EC_TX |
				RDD_FLAG_UART_EC_BITBANG);

	/* Disable based on capabilities */
	if (!ccd_is_cap_enabled(CCD_CAP_AP_TX_CR50_RX))
		flags_want &= ~RDD_FLAG_UART_AP;
	if (!ccd_is_cap_enabled(CCD_CAP_AP_RX_CR50_TX))
		flags_want &= ~RDD_FLAG_UART_AP_TX;
	if (!ccd_is_cap_enabled(CCD_CAP_EC_TX_CR50_RX))
		flags_want &= ~RDD_FLAG_UART_EC;
	if (!ccd_is_cap_enabled(CCD_CAP_EC_RX_CR50_TX))
		flags_want &= ~(RDD_FLAG_UART_EC_TX | RDD_FLAG_UART_EC_BITBANG);
	if (!ccd_is_cap_enabled(CCD_CAP_I2C))
		flags_want &= ~RDD_FLAG_I2C;

	/* EC UART blocked by bit-banging */
	if (flags_want & RDD_FLAG_UART_EC_BITBANG)
		flags_want &= ~(RDD_FLAG_UART_EC | RDD_FLAG_UART_EC_TX);

	/* UARTs are either RX-only or RX+TX, so no RX implies no TX */
	if (!(flags_want & RDD_FLAG_UART_AP))
		flags_want &= ~RDD_FLAG_UART_AP_TX;
	if (!(flags_want & RDD_FLAG_UART_EC))
		flags_want &= ~RDD_FLAG_UART_EC_TX;

	/* If no change, we're done */
	if (flags_now == flags_want)
		return;

	CPRINTS("RDD 0x%x -> 0x%x", flags_now, flags_want);

	/* Handle turning things off */
	delta = flags_now & ~flags_want;
	if (delta & RDD_FLAG_UART_AP)
		uartn_disable(UART_AP);
	if (delta & RDD_FLAG_UART_AP_TX)
		uartn_tx_disconnect(UART_AP);
	if (delta & RDD_FLAG_UART_EC)
		uartn_disable(UART_EC);
	if (delta & RDD_FLAG_UART_EC_TX)
		uartn_tx_disconnect(UART_EC);
#ifdef CONFIG_UART_BITBANG
	if (delta & RDD_FLAG_UART_EC_BITBANG)
		uart_bitbang_disable(UART_EC);
#endif
	if (delta & RDD_FLAG_I2C)
		usb_i2c_board_disable();

	/* Handle turning things on */
	delta = flags_want & ~flags_now;
	if (delta & RDD_FLAG_UART_AP)
		uartn_enable(UART_AP);
	if (delta & RDD_FLAG_UART_AP_TX)
		uartn_tx_connect(UART_AP);
	if (delta & RDD_FLAG_UART_EC)
		uartn_enable(UART_EC);
	if (delta & RDD_FLAG_UART_EC_TX)
		uartn_tx_connect(UART_EC);
#ifdef CONFIG_UART_BITBANG
	if (delta & RDD_FLAG_UART_EC_BITBANG)
		uart_bitbang_enable(UART_EC);
#endif
	if (delta & RDD_FLAG_I2C)
		usb_i2c_board_enable();
}
DECLARE_DEFERRED(rdd_change_hook);

void rdd_update_state(void)
{
	/*
	 * Use a deferred call to serialize changes from CCD state, RDD
	 * attach/detach, EC/AP startup or shutdown, etc.
	 */
	hook_call_deferred(&rdd_change_hook_data, 0);
}

static void clear_keepalive(void)
{
	keep_ccd_enabled = 0;
	ccprintf("Cleared CCD keepalive\n");
}

static int command_ccd(int argc, char **argv)
{
	uint32_t flags;
	int val;

	if (argc > 1) {
		if (console_is_restricted())
			return EC_ERROR_ACCESS_DENIED;

		if (!parse_bool(argv[argc - 1], &val))
			return argc == 2 ? EC_ERROR_PARAM1 : EC_ERROR_PARAM2;

		if (!strcasecmp("keepalive", argv[1])) {
			if (val) {
				/* Make sure ccd is enabled */
				if (!ccd_is_enabled())
					rdd_attached();

				keep_ccd_enabled = 1;
				ccprintf("Warning CCD will remain "
					 "enabled until it is "
					 "explicitly disabled.\n");
			} else {
				clear_keepalive();
			}
		} else if (argc == 2) {
			if (val) {
				rdd_attached();
			} else {
				if (keep_ccd_enabled)
					clear_keepalive();

				rdd_detached();
			}
		} else
			return EC_ERROR_PARAM1;
	}

	/* Some of these are redundant with the flags below */
	ccprintf("CCD:     %s\n",
		keep_ccd_enabled ? "forced enable" :
		ccd_is_enabled() ? "enabled" : "disabled");
	ccprintf("AP UART: %s\n",
		 uartn_is_enabled(UART_AP) ?
		 uart_tx_is_connected(UART_AP) ? "RX+TX" : "RX" : "disabled");
	ccprintf("EC UART: %s\n",
		 uartn_is_enabled(UART_EC) ?
		 uart_tx_is_connected(UART_EC) ? "RX+TX" : "RX" : "disabled");
	ccprintf("I2C:     %s\n",
		 usb_i2c_board_is_enabled() ? "enabled" : "disabled");
	ccprintf("Servo:   %s\n",
		 servo_is_connected() ? "connected" : "disconnected");
	ccprintf("AP:      %s\n",
		 ap_is_connected() ? "connected" : "disconnected");
	ccprintf("EC:      %s\n",
		 ec_is_connected() ? "connected" : "disconnected");

	ccprintf("Flags:  ");
	flags = rdd_get_flags();
	if (flags & RDD_FLAG_UART_AP)
		ccprintf(" APRX");
	if (flags & RDD_FLAG_UART_AP_TX)
		ccprintf(" APTX");
	if (flags & RDD_FLAG_UART_EC)
		ccprintf(" ECRX");
	if (flags & RDD_FLAG_UART_EC_TX)
		ccprintf(" ECTX");
	if (flags & RDD_FLAG_UART_EC_BITBANG)
		ccprintf(" ECBB");
	if (flags & RDD_FLAG_I2C)
		ccprintf(" I2C");
	ccprintf("\n");

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(ccd, command_ccd,
			"[keepalive] [<BOOLEAN>]",
			"Get/set the case closed debug state");

static int command_sys_rst(int argc, char **argv)
{
	int val;
	char *e;
	int ms = 20;

	if (argc > 1) {
		if (!ccd_is_cap_enabled(CCD_CAP_REBOOT_EC_AP))
			return EC_ERROR_ACCESS_DENIED;

		if (!strcasecmp("pulse", argv[1])) {
			if (argc == 3) {
				ms = strtoi(argv[2], &e, 0);
				if (*e)
					return EC_ERROR_PARAM2;
			}
			ccprintf("Pulsing AP reset for %dms\n", ms);
			assert_sys_rst();
			msleep(ms);
			deassert_sys_rst();
		} else if (parse_bool(argv[1], &val)) {
			if (val)
				assert_sys_rst();
			else
				deassert_sys_rst();
		} else
			return EC_ERROR_PARAM1;
	}

	ccprintf("SYS_RST_L is %s\n", is_sys_rst_asserted() ?
		 "asserted" : "deasserted");

	return EC_SUCCESS;

}
DECLARE_SAFE_CONSOLE_COMMAND(sysrst, command_sys_rst,
	"[pulse [time] | <BOOLEAN>]",
	"Assert/deassert SYS_RST_L to reset the AP");

static int command_ec_rst(int argc, char **argv)
{
	int val;

	if (argc > 1) {
		if (!ccd_is_cap_enabled(CCD_CAP_REBOOT_EC_AP))
			return EC_ERROR_ACCESS_DENIED;

		if (!strcasecmp("pulse", argv[1])) {
			ccprintf("Pulsing EC reset\n");
			assert_ec_rst();
			usleep(200);
			deassert_ec_rst();
		} else if (parse_bool(argv[1], &val)) {
			if (val)
				assert_ec_rst();
			else
				deassert_ec_rst();
		} else
			return EC_ERROR_PARAM1;
	}

	ccprintf("EC_RST_L is %s\n", is_ec_rst_asserted() ?
		 "asserted" : "deasserted");

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(ecrst, command_ec_rst,
	"[pulse | <BOOLEAN>]",
	"Assert/deassert EC_RST_L to reset the EC (and AP)");

static int command_powerbtn(int argc, char **argv)
{
	char *e;
	int ms = 200;

	if (argc > 1) {
		if (!strcasecmp("pulse", argv[1])) {
			if (argc == 3) {
				ms = strtoi(argv[2], &e, 0);
				if (*e)
					return EC_ERROR_PARAM2;
			}

			ccprintf("Force %dms power button press\n", ms);

			rbox_powerbtn_press();
			msleep(ms);
			rbox_powerbtn_release();
		} else if (!strcasecmp("press", argv[1])) {
			rbox_powerbtn_press();
		} else if (!strcasecmp("release", argv[1])) {
			rbox_powerbtn_release();
		} else
			return EC_ERROR_PARAM1;
	}

	ccprintf("powerbtn: %s\n",
		 rbox_powerbtn_override_is_enabled() ? "forced press" :
		 rbox_powerbtn_is_pressed() ? "pressed\n" : "released\n");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(powerbtn, command_powerbtn,
			"[pulse [ms] | press | release]",
			"get/set the state of the power button");
