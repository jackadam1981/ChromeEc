/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "adc.h"
#include "button.h"
#include "charge_manager.h"
#include "charge_state_v2.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "cros_board_info.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "power_button.h"
#include "hooks.h"
#include "power.h"
#include "switch.h"
#include "throttle_ap.h"
#include "usbc_config.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/******************************************************************************/
/* USB-A charging control */

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USBA,
};
BUILD_ASSERT(ARRAY_SIZE(usb_port_enable) == USB_PORT_COUNT);

/******************************************************************************/
/* ADP_ID control */
struct adpater_id_params tio1_power[] = {
	{
	.min_voltage = 0x3FF,
	.max_voltage = 0x3FF,
	.watt = 120000,
	.obp95 = 0x2D3,
	.obp85 = 0x286
	},
};

struct adpater_id_params tio2_power[] = {
	{
	.min_voltage = 0,
	.max_voltage = 0x19,
	.watt = 170000,
	.obp95 = 0x400,
	.obp85 = 0x394
	},
	{
	.min_voltage = 0x19,
	.max_voltage = 0x34,
	.watt = 45000,
	.obp95 = 0x10F,
	.obp85 = 0x0F2
	},
	{
	.min_voltage = 0x49,
	.max_voltage = 0x69,
	.watt = 65000,
	.obp95 = 0x187,
	.obp85 = 0x15E
	},
	{
	.min_voltage = 0xC1,
	.max_voltage = 0xDD,
	.watt = 120000,
	.obp95 = 0x2D3,
	.obp85 = 0x286
	},
	{
	.min_voltage = 0x8C,
	.max_voltage = 0xAE,
	.watt = 150000,
	.obp95 = 0x387,
	.obp85 = 0x328
	},
	{
	.min_voltage = 0x182,
	.max_voltage = 0x199,
	.watt = 170000,
	.obp95 = 0x400,
	.obp85 = 0x394
	},
};

struct adpater_id_params tiny_power[] = {
	{
	.min_voltage = 0x19,
	.max_voltage = 0x34,
	.watt = 45000,
	.obp95 = 0x10F,
	.obp85 = 0x0F2
	},
	{
	.min_voltage = 0x49,
	.max_voltage = 0x69,
	.watt = 65000,
	.obp95 = 0x187,
	.obp85 = 0x15E
	},
	{
	.min_voltage = 0x8C,
	.max_voltage = 0xAE,
	.watt = 90000,
	.obp95 = 0x21E,
	.obp85 = 0x1E5
	},
	{
	.min_voltage = 0xC1,
	.max_voltage = 0xDD,
	.watt = 120000,
	.obp95 = 0x2D3,
	.obp85 = 0x286
	},
	{
	.min_voltage = 0xED,
	.max_voltage = 0x11C,
	.watt = 135000,
	.obp95 = 0x32D,
	.obp85 = 0x2D7
	},
	{
	.min_voltage = 0x135,
	.max_voltage = 0x16A,
	.watt = 150000,
	.obp95 = 0x387,
	.obp85 = 0x328
	},
	{
	.min_voltage = 0x182,
	.max_voltage = 0x1BD,
	.watt = 170000,
	.obp95 = 0x400,
	.obp85 = 0x394
	},
	{
	.min_voltage = 0x27B,
	.max_voltage = 0x2CB,
	.watt = 230000,
	.obp95 = 0x569,
	.obp85 = 0x4D7
	},
};

struct adpater_id_params *power_type[8];

static int adp_id_value;
static int adp_id_value_debounce;
static int adp_type;
static int adp_finial_adc_value;
static int adp_obp95;
static int adp_obp85;

DECLARE_DEFERRED(adp_id_deferred);
DECLARE_DEFERRED(board_obp_deferred);

void adp_id_deferred(void)
{
	int i = 0;

	adp_id_value = adc_read_channel(ADC_ADP_ID);

	if (!adp_id_value_debounce) {
		adp_id_value_debounce = adp_id_value;
		/* for delay the 400ms to get the next APD_ID value */
		hook_call_deferred(&adp_id_deferred_data, 400 * MSEC);
	} else if (adp_id_value_debounce == 0x3FF && adp_id_value == 0x3FF) {
		adp_finial_adc_value = adp_id_value;
		adp_type = TIO1;
	} else if (adp_id_value_debounce < 0x3FF && adp_id_value == 0x3FF) {
		adp_finial_adc_value = adp_id_value_debounce;
		adp_type = TIO2;
	} else if (adp_id_value_debounce < 0x3FF && adp_id_value < 0x3FF) {
		adp_finial_adc_value = adp_id_value;
		adp_type = TINY;
	}

	switch (adp_type) {
	case TIO1:
		memcpy(power_type, &tio1_power, sizeof(tio1_power));
		break;
	case TIO2:
		memcpy(power_type, &tio2_power, sizeof(tio2_power));
		break;
	case TINY:
		memcpy(power_type, &tiny_power, sizeof(tiny_power));
		break;
	}

	for (i = 0; i < sizeof(power_type) && adp_type; i++) {
		if (adp_finial_adc_value <= power_type[i]->max_voltage) {
			adp_obp95 = power_type[i]->obp95;
			adp_obp85 = power_type[i]->obp85;
			break;
		}
	}
}

void board_obp_deferred(void)
{
	int imon_value = adc_read_channel(ADC_PWR_IN_IMON);
	int active = 1;

	if (imon_value > adp_obp95)
		/* Trigger Prochot */
		active = 0;
	else if (imon_value < adp_obp85)
		/* Release Prochot */
		active = 1;

	gpio_set_level(GPIO_EC_PROCHOT_ODL, active);
}
DECLARE_HOOK(HOOK_TICK, board_obp_deferred, HOOK_PRIO_DEFAULT);

int board_set_active_charge_port(int port)
{
	CPRINTS("Requested charge port change to %d", port);

	/*
	 * The charge manager may ask us to switch to no charger if we're
	 * running off USB-C only but upstream doesn't support PD. It requires
	 * that we accept this switch otherwise it triggers an assert and EC
	 * reset; it's not possible to boot the AP anyway, but we want to avoid
	 * resetting the EC so we can continue to do the "low power" LED blink.
	 */
	if (port == CHARGE_PORT_NONE)
		return EC_SUCCESS;

	if (port < 0 || CHARGE_PORT_COUNT <= port)
		return EC_ERROR_INVAL;

	if (port == charge_manager_get_active_charge_port())
		return EC_SUCCESS;

	/* Don't charge from a source port */
	if (board_vbus_source_enabled(port))
		return EC_ERROR_INVAL;

	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		int bj_active, bj_requested;

		if (charge_manager_get_active_charge_port() != CHARGE_PORT_NONE)
			/* Change is only permitted while the system is off */
			return EC_ERROR_INVAL;

		/*
		 * Current setting is no charge port but the AP is on, so the
		 * charge manager is out of sync (probably because we're
		 * reinitializing after sysjump). Reject requests that aren't
		 * in sync with our outputs.
		 */
		bj_active = !gpio_get_level(GPIO_EN_PPVAR_BJ_ADP_L);
		bj_requested = port == CHARGE_PORT_BARRELJACK;
		if (bj_active != bj_requested)
			return EC_ERROR_INVAL;
	}

	CPRINTS("New charger p%d", port);

	switch (port) {
	case CHARGE_PORT_TYPEC0:
	case CHARGE_PORT_TYPEC1:
	case CHARGE_PORT_TYPEC2:
		gpio_set_level(GPIO_EN_PPVAR_BJ_ADP_L, 1);
		break;
	case CHARGE_PORT_BARRELJACK:
		/* Make sure BJ adapter is sourcing power */
		if (gpio_get_level(GPIO_BJ_ADP_PRESENT_ODL))
			return EC_ERROR_INVAL;
		gpio_set_level(GPIO_EN_PPVAR_BJ_ADP_L, 0);
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
				int max_ma, int charge_mv)
{
}

/******************************************************************************/
/*
 * Barrel jack power supply handling
 *
 * EN_PPVAR_BJ_ADP_L must default active to ensure we can power on when the
 * barrel jack is connected, and the USB-C port can bring the EC up fine in
 * dead-battery mode. Both the USB-C and barrel jack switches do reverse
 * protection, so we're safe to turn one on then the other off- but we should
 * only do that if the system is off since it might still brown out.
 */

/*
 * Barrel-jack power adapter ratings.
 */
static const struct {
	int voltage;
	int current;
} bj_power[] = {
	{ /* 0 - 135W (also default) */
	.voltage = 19500,
	.current = 6920
	},
	{ /* 1 - 230W */
	.voltage = 19500,
	.current = 11800
	},
};

static unsigned int ec_config_get_bj_power(void)
{
	uint32_t fw_config;
	unsigned int bj;

	cbi_get_fw_config(&fw_config);
	bj = (fw_config & EC_CFG_BJ_POWER_MASK) >> EC_CFG_BJ_POWER_L;
	/* Out of range value defaults to 0 */
	if (bj >= ARRAY_SIZE(bj_power))
		bj = 0;
	return bj;
}

#define ADP_DEBOUNCE_MS		1000  /* Debounce time for BJ plug/unplug */
/* Debounced connection state of the barrel jack */
static int8_t adp_connected = -1;
static void adp_connect_deferred(void)
{
	struct charge_port_info pi = { 0 };
	int connected = !gpio_get_level(GPIO_BJ_ADP_PRESENT_ODL);

	/* Debounce */
	if (connected == adp_connected)
		return;
	if (connected) {
		unsigned int bj = ec_config_get_bj_power();

		pi.voltage = bj_power[bj].voltage;
		pi.current = bj_power[bj].current;
	}
	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, &pi);
	adp_connected = connected;
}
DECLARE_DEFERRED(adp_connect_deferred);

/* IRQ for BJ plug/unplug. It shouldn't be called if BJ is the power source. */
void adp_connect_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&adp_connect_deferred_data, ADP_DEBOUNCE_MS * MSEC);
}

static void adp_state_init(void)
{
	ASSERT(CHARGE_PORT_ENUM_COUNT == CHARGE_PORT_COUNT);
	/*
	 * Initialize all charge suppliers to 0. The charge manager waits until
	 * all ports have reported in before doing anything.
	 */
	for (int i = 0; i < CHARGE_PORT_COUNT; i++) {
		for (int j = 0; j < CHARGE_SUPPLIER_COUNT; j++)
			charge_manager_update_charge(j, i, NULL);
	}

	/* Report charge state from the barrel jack. */
	adp_connect_deferred();
}
DECLARE_HOOK(HOOK_INIT, adp_state_init, HOOK_PRIO_CHARGE_MANAGER_INIT + 1);

static void board_init(void)
{
	gpio_enable_interrupt(GPIO_BJ_ADP_PRESENT_ODL);
	hook_call_deferred(&adp_id_deferred_data, 200 * MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
