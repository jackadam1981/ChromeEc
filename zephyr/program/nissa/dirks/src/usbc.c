/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "board.h"
#include "charge_state.h"
#include "chipset.h"
#include "driver/charger/sm5803.h"
#include "driver/retimer/ps8811.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/ps8xxx_public.h"
#include "driver/tcpm/tcpci.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "system.h"
#include "temp_sensor/temp_sensor.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"

#include <zephyr/logging/log.h>

#include <ap_power/ap_power.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/*
 * Enable interrupts
 */
static void board_init(void)
{
	/*
	 * Enable USB-C interrupts.
	 */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0));
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

__override int extpower_is_present(void)
{
	/*
	 * There's no battery, so running this method implies we have power.
	 */
	return 1;
}

int board_vbus_source_enabled(int port)
{
	if (port != CHARGE_PORT_TYPEC0)
		return 0;

	return ppc_is_sourcing_vbus(port);
}

/* Vconn control for integrated ITE TCPC */
void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/*
	 * We ignore the cc_pin and PPC vconn because polarity and PPC vconn
	 * should already be set correctly in the PPC driver via the pd
	 * state machine.
	 */
}

void pd_power_supply_reset(int port)
{
	int prev_en;

	if (port < 0 || port >= board_get_usb_pd_port_count())
		return;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS source */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	if (port < 0 || port > board_get_usb_pd_port_count()) {
		LOG_WRN("Port C%d does not exist, cannot enable VBUS", port);
		return EC_ERROR_INVAL;
	}

	/* Disable charging */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv)
		return rv;

	pd_set_vbus_discharge(port, 0);

	/* Enable VBUS source */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}

/* LCOV_EXCL_START function does nothing, but is required for build */
void board_reset_pd_mcu(void)
{
	/*
	 * Nothing to do.  TCPC C0 is internal.
	 */
}
/* LCOV_EXCL_STOP */

/*
 * Handle charger interrupts in the PD task. Not doing so can lead to a priority
 * inversion where we fail to respond to TCPC alerts quickly enough because we
 * don't get another edge on a shared IRQ until the other interrupt is cleared
 * (or the IRQ is polled again), which happens in lower-priority tasks: the
 * high-priority type-C handler is thus blocked on the lower-priority one(s).
 *
 * To avoid that, we run charger and BC1.2 interrupts synchronously alongside
 * PD interrupts so they have the same priority.
 */
void board_process_pd_alert(int port)
{
	/*
	 * Port 0 doesn't use an external TCPC, so its interrupts don't need
	 * this special handling.
	 */
	return;
}

int pd_snk_is_vbus_provided(int port)
{
	if (port != CHARGE_PORT_TYPEC0)
		return 0;

	return ppc_is_vbus_present(port);
}

__override int board_get_vbus_voltage(int port)
{
	return adc_read_channel(ADC_VBUS);
}

/* USB-A ports */
enum usba_port { USBA_PORT_A1, USBA_PORT_COUNT };
const struct usb_mux usba_ps8811[] = {
	[USBA_PORT_A1] = {
		.usb_port = USBA_PORT_A1,
		.i2c_port = I2C_PORT_NODELABEL(i2c4),
		.i2c_addr_flags = PS8811_I2C_ADDR_FLAGS0,
	},
};

void usba_retimer_init(void)
{
	int rv;
	const struct usb_mux *me = &usba_ps8811[USBA_PORT_A1];

	/* Set offset 0x66 value 0x20 */
	rv = ps8811_i2c_write(me, PS8811_REG_PAGE1,
			      PS8811_REG1_USB_CHAN_A_SWING, 0x20);

	if (rv) {
		LOG_WRN("A1: PS8811 retimer response fail!");
	}
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, usba_retimer_init, HOOK_PRIO_DEFAULT);

struct typec_ilim_step {
	int on;
	int off;
	enum tcpc_rp_value typec_rp;
};

static const struct typec_ilim_step typec_ilim_table[] = {
	{ .on = 0, .off = 0, .typec_rp = TYPEC_RP_3A0 },
	{ .on = 84, .off = 76, .typec_rp = TYPEC_RP_1A5 },
	{ .on = 90, .off = 82, .typec_rp = TYPEC_RP_USB },
};

#define NUM_TYPEC_ILIM_LEVELS ARRAY_SIZE(typec_ilim_table)

static void typec_ilim_control(void)
{
	int rv;
	int chg_temp_c;
	int thermal_sensor0;
	bool level_changed = false;
	static int current_level;
	static int prev_tmp;

	rv = temp_sensor_read(TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(temp_ambient)),
			      &thermal_sensor0);
	chg_temp_c = K_TO_C(thermal_sensor0);

	if (rv != EC_SUCCESS)
		return;

	if (chg_temp_c < prev_tmp) {
		if (chg_temp_c <= typec_ilim_table[current_level].off) {
			current_level = current_level - 1;
			/* Prevent level always minus 0 */
			if (current_level < 0)
				current_level = 0;
			else
				level_changed = true;
		}
	} else if (chg_temp_c > prev_tmp) {
		if (chg_temp_c >= typec_ilim_table[current_level + 1].on) {
			current_level = current_level + 1;
			/* Prevent level always over table steps */
			if (current_level >= NUM_TYPEC_ILIM_LEVELS)
				current_level = NUM_TYPEC_ILIM_LEVELS - 1;
			else
				level_changed = true;
		}
	}

	prev_tmp = chg_temp_c;

	if (ppc_is_sourcing_vbus(0) && level_changed) {
		enum tcpc_rp_value rp =
			typec_ilim_table[current_level].typec_rp;

		LOG_INF("Temp changed to %dC: Rp=%d", chg_temp_c, rp);
		ppc_set_vbus_source_current_limit(0, rp);
		tcpm_select_rp_value(0, rp);
		pd_update_contract(0);
	}
}
DECLARE_HOOK(HOOK_SECOND, typec_ilim_control, HOOK_PRIO_TEMP_SENSOR_DONE);
