/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "driver/ppc/syv682x_public.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/ps8xxx_public.h"
#include "driver/tcpm/tcpci.h"
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "telith_typec_num.h"
#include "system.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "cros_cbi.h"

#include <zephyr/logging/log.h>

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

#ifdef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
const struct cc_para_t *board_get_cc_tuning_parameter(enum usbpd_port port)
{
	const static struct cc_para_t
		cc_parameter[CONFIG_USB_PD_ITE_ACTIVE_PORT_COUNT] = {
			{
				.rising_time =
					IT83XX_TX_PRE_DRIVING_TIME_1_UNIT,
				.falling_time =
					IT83XX_TX_PRE_DRIVING_TIME_2_UNIT,
			},
			{
				.rising_time =
					IT83XX_TX_PRE_DRIVING_TIME_1_UNIT,
				.falling_time =
					IT83XX_TX_PRE_DRIVING_TIME_2_UNIT,
			},
		};

	return &cc_parameter[port];
}
#endif

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/*
	 * We ignore the cc_pin and PPC vconn because polarity and PPC vconn
	 * should already be set correctly in the PPC driver via the pd
	 * state machine.
	 */
}

enum usbc_port { USBC_PORT_C0 = 0, USBC_PORT_C1, USBC_PORT_COUNT };

/* Used by USB charger task with CONFIG_USB_PD_5V_EN_CUSTOM */
int board_is_sourcing_vbus(int port)
{
	return ppc_is_sourcing_vbus(port);
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

int board_set_active_charge_port(int port)
{
	int is_valid_port = board_is_usb_pd_port_present(port);
	int i;

	if (port == CHARGE_PORT_NONE) {
		/* Disable all ports. */
		for (i = 0; i < ppc_cnt; i++) {
			/*
			 * Do not return early if one fails otherwise we can
			 * get into a boot loop assertion failure.
			 */
			if (ppc_vbus_sink_enable(i, 0))
				CPRINTSUSB("Disabling C%d as sink failed.", i);
		}

		return EC_SUCCESS;
	} else if (!is_valid_port) {
		return EC_ERROR_INVAL;
	}

	/* Check if the port is sourcing VBUS. */
	if (board_is_sourcing_vbus(port)) {
		CPRINTFUSB("Skip enable C%d", port);
		return EC_ERROR_INVAL;
	}

	CPRINTSUSB("New charge port: C%d", port);

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < ppc_cnt; i++) {
		if (i == port)
			continue;

		if (ppc_vbus_sink_enable(i, 0))
			CPRINTSUSB("C%d: sink path disable failed.", i);
	}

	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTSUSB("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static void notify_power_change(void)
{
	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}
DECLARE_DEFERRED(notify_power_change);

void pd_power_supply_reset(int port)
{
	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (IS_ENABLED(CONFIG_USB_PD_DISCHARGE))
		pd_set_vbus_discharge(port, 1);

	/* defer pd_send_host_event to save ~2ms for PD compliance */
	hook_call_deferred(&notify_power_change_data, 0);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	if (port >= CONFIG_USB_PD_PORT_MAX_COUNT) {
		return EC_ERROR_INVAL;
	}

	/* Disable charging. */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv) {
		LOG_WRN("C%d failed to disable sinking: %d", port, rv);
		return rv;
	}
	if (IS_ENABLED(CONFIG_USB_PD_DISCHARGE)) {
		pd_set_vbus_discharge(port, 0);
	}

	/* Provide Vbus. */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv) {
		LOG_WRN("C%d failed to enable VBUS sourcing: %d", port, rv);
		return rv;
	}

	/* defer pd_send_host_event to save ~2ms for PD compliance */
	hook_call_deferred(&notify_power_change_data, 0);

	return EC_SUCCESS;
}

__override int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

__override void typec_set_source_current_limit(int port, enum tcpc_rp_value rp)
{
	int rv;
	const int current = rp == TYPEC_RP_3A0 ? 3000 : 1500;

	rv = ppc_set_vbus_source_current_limit(port, rp);
	if (rv != EC_SUCCESS) {
		LOG_WRN("Failed to set source ilimit on port %d to %d: %d",
			port, current, rv);
	}
}

/* LCOV_EXCL_START function does nothing, but is required for build */
void board_reset_pd_mcu(void)
{
	/*
	 * Do nothing. The integrated TCPC for C0 lacks a dedicated reset
	 * command, and C1 (if present) doesn't have a reset pin connected
	 * to the EC.
	 */
}
/* LCOV_EXCL_STOP */

static uint8_t cached_usb_pd_port_count;

__override uint8_t board_get_usb_pd_port_count(void)
{
	__ASSERT(cached_usb_pd_port_count != 0,
		 "sub-board detection did not run before a port count request");
	/* LCOV_EXCL_START - will not happen due to board_init is
	 * HOOK_PRIO_DEFAULT
	 */
	if (cached_usb_pd_port_count == 0)
		LOG_WRN("USB PD Port count not initialized!");
	/* LCOV_EXCL_STOP */
	return cached_usb_pd_port_count;
}

test_export_static enum telith_typec_num nissa_cached_sub_board =
	TELITH_TYPEC_UNKNOWN;

enum telith_typec_num telith_get_typec_num(void)
{
	int ret;
	uint32_t val;

	/*
	 * Return cached value.
	 */
	if (nissa_cached_sub_board != TELITH_TYPEC_UNKNOWN)
		return nissa_cached_sub_board;

	nissa_cached_sub_board = TELITH_TYPEC_NONE; /* Defaults to none */
	ret = cros_cbi_get_fw_config(FW_SUB_BOARD, &val);
	if (ret != 0) {
		LOG_WRN("Error retrieving CBI FW_CONFIG field %d",
			FW_SUB_BOARD);
		return nissa_cached_sub_board;
	}
	switch (val) {
	default:
		LOG_WRN("No sub-board defined");
		break;
	case FW_SUB_BOARD_1:
		nissa_cached_sub_board = TELITH_TYPEC_ONE;
		LOG_INF("Only one USB type C");
		break;
	}
	return nissa_cached_sub_board;
}

test_export_static void board_usb_pd_count_init(void)
{
	/* Check CBI to determine actual port count */
	switch (telith_get_typec_num()) {
	default:
		cached_usb_pd_port_count = 2;
		break;

	case TELITH_TYPEC_ONE:
		cached_usb_pd_port_count = 1;
		break;
	}
}
/*
 * Make sure setup is done after EEPROM is readable.
 */
DECLARE_HOOK(HOOK_INIT, board_usb_pd_count_init, HOOK_PRIO_INIT_I2C);
