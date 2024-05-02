/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Veluza daughter board detection */
#include "baseboard_usbc_config.h"
#include "console.h"
#include "cros_cbi.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "variant_db_detection.h"

#include <zephyr/drivers/gpio.h>

#include <ap_power/ap_power.h>

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

enum veluza_db_type {
	VELUZA_DB_UNINIT = -1,
	VELUZA_DB_HDMI,
	VELUZA_DB_NO_HDMI,
	VELUZA_DB_COUNT,
};

#ifdef TEST_BUILD
uint32_t dp_status[CONFIG_USB_PD_PORT_MAX_COUNT];
#endif

static void veluza_db_config(enum veluza_db_type type)
{
	switch (type) {
	case VELUZA_DB_HDMI:
		/* EC_X_GPIO1 */
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_en_hdmi_pwr),
				      GPIO_OUTPUT_HIGH);
		/* X_EC_GPIO2 */
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_ec_dp_hpd),
				      GPIO_INPUT);
		gpio_enable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_x_ec_gpio2));
		/* EC_X_GPIO3 */
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_pwrdn_odl),
				      GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN);
		return;
	case VELUZA_DB_NO_HDMI:
		/* EC_X_GPIO1 */
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_en_hdmi_pwr),
				      GPIO_OUTPUT_LOW);
		/* EC_X_GPIO3 */
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_pwrdn_odl),
				      GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN);
		return;
	default:
		break;
	}
}

enum veluza_db_type veluza_get_db_type(void)
{
	static enum veluza_db_type db = VELUZA_DB_UNINIT;

	if (db != VELUZA_DB_UNINIT) {
		return db;
	}

	if (!gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_hdmi_prsnt_odl))) {
		db = VELUZA_DB_HDMI;
	} else {
		db = VELUZA_DB_NO_HDMI;
	}

	veluza_db_config(db);

	switch (db) {
	case VELUZA_DB_HDMI:
		CPRINTS("Detect %s DB", "HDMI");
		break;
	case VELUZA_DB_NO_HDMI:
		CPRINTS("Detect %s DB", "No HDMI");
		break;
	default:
		CPRINTS("DB UNINIT");
		break;
	}

	return db;
}

static void veluza_db_init(void)
{
	veluza_get_db_type();
}
DECLARE_HOOK(HOOK_INIT, veluza_db_init, HOOK_PRIO_PRE_I2C);

/**
 * Handle PS185 HPD changing state.
 */
void ps185_hdmi_hpd_mux_set(void)
{
	const int hpd =
		gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_ec_dp_hpd));

	if (!corsola_is_dp_muxable(USBC_PORT_C0)) {
		return;
	}

	if (hpd && !(usb_mux_get(USBC_PORT_C0) & USB_PD_MUX_DP_ENABLED)) {
		dp_status[USBC_PORT_C0] =
			VDO_DP_STATUS(0, /* HPD IRQ  ... not applicable */
				      0, /* HPD level ... not applicable */
				      0, /* exit DP? ... no */
				      0, /* usb mode? ... no */
				      0, /* multi-function ... no */
				      1, /* DP enabled ... yes */
				      0, /* power low?  ... no */
				      (!!DP_FLAGS_DP_ON));
		/* update C1 virtual mux */
		usb_mux_set(USBC_PORT_C0, USB_PD_MUX_DP_ENABLED,
			    USB_SWITCH_DISCONNECT,
			    0 /* polarity, don't care */);
		CPRINTS("HDMI plug");
	}
}

static void ps185_hdmi_hpd_deferred(void)
{
	const int hpd =
		gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_ec_dp_hpd));

	if (!hpd && (usb_mux_get(USBC_PORT_C0) & USB_PD_MUX_DP_ENABLED)) {
		dp_status[USBC_PORT_C0] =
			VDO_DP_STATUS(0, /* HPD IRQ  ... not applicable */
				      0, /* HPD level ... not applicable */
				      0, /* exit DP? ... no */
				      0, /* usb mode? ... no */
				      0, /* multi-function ... no */
				      0, /* DP enabled ... no */
				      0, /* power low?  ... no */
				      (!DP_FLAGS_DP_ON));
		usb_mux_set(USBC_PORT_C0, USB_PD_MUX_NONE,
			    USB_SWITCH_DISCONNECT,
			    0 /* polarity, don't care */);
		CPRINTS("HDMI unplug");

		return;
	}

	ps185_hdmi_hpd_mux_set();
}
DECLARE_DEFERRED(ps185_hdmi_hpd_deferred);

#define HPD_SINK_HPD_SING (500 * MSEC)
#define HPD_SINK_ABSENCE_DEBOUNCE (2 * MSEC)

static void hdmi_hpd_low(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_ap_dp_hpd_odl), 0);
	hook_call_deferred(&ps185_hdmi_hpd_deferred_data, HPD_SINK_HPD_SING);
}
DECLARE_DEFERRED(hdmi_hpd_low);
static void hdmi_hpd_high(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_ap_dp_hpd_odl), 1);
	hook_call_deferred(&hdmi_hpd_low_data, HPD_SINK_HPD_SING);
}
DECLARE_DEFERRED(hdmi_hpd_high);

static void hdmi_hpd_interrupt_deferred(void)
{
	const int hpd =
		gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_ec_dp_hpd));

	/* C0 DP is muxed, we should not send HPD to the AP */
	if (!corsola_is_dp_muxable(USBC_PORT_C0)) {
		if (hpd) {
			CPRINTS("port is already muxed.");
		}
		return;
	}

	if (hpd && !(usb_mux_get(USBC_PORT_C0) & USB_PD_MUX_DP_ENABLED)) {
		/* set dp_aux_path_sel first, and configure the usb_mux in the
		 * deferred hook to prevent from dead locking.
		 */
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(dp_aux_path_sel), hpd);
		hook_call_deferred(&hdmi_hpd_high_data, HPD_SINK_HPD_SING);
	}

	svdm_set_hpd_gpio(USBC_PORT_C0, hpd);
}
DECLARE_DEFERRED(hdmi_hpd_interrupt_deferred);

void hdmi_hpd_interrupt(enum gpio_signal signal)
{
	const int hpd =
		gpio_pin_get_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_ec_dp_hpd));

	if (!hpd) {
		hook_call_deferred(&ps185_hdmi_hpd_deferred_data,
				   HPD_SINK_ABSENCE_DEBOUNCE);
	} else {
		hook_call_deferred(&ps185_hdmi_hpd_deferred_data, -1);
	}

	hook_call_deferred(&hdmi_hpd_interrupt_deferred_data, 0);
}

static void board_hdmi_handler(struct ap_power_ev_callback *cb,
			       struct ap_power_ev_data data)
{
	int value;

	switch (data.event) {
	default:
		return;

	case AP_POWER_RESUME:
		value = 1;
		break;

	case AP_POWER_SUSPEND:
		value = 0;
		break;
	}
	gpio_pin_set_dt(GPIO_DT_FROM_ALIAS(gpio_en_hdmi_pwr), value);
	gpio_pin_set_dt(GPIO_DT_FROM_ALIAS(gpio_ps185_pwrdn_odl), value);
}

static void tasks_init_deferred(void)
{
	if (veluza_get_db_type() == VELUZA_DB_HDMI) {
		/* If the HDMI port is plugged on-boot, and the usb_mux won't
		 * be configured before the task inited.  Re-invoke the
		 * HPD configuration after task inited.
		 */
		ps185_hdmi_hpd_mux_set();
	}
}
DECLARE_DEFERRED(tasks_init_deferred);

test_export_static void board_x_ec_gpio2_init(void)
{
	static struct ppc_drv virtual_ppc_drv = { 0 };
	static struct tcpm_drv virtual_tcpc_drv = { 0 };

	/* type-c: USB_C0_PPC_INT_ODL / hdmi: PS185_EC_DP_HPD */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_x_ec_gpio2));

	if (veluza_get_db_type() == VELUZA_DB_HDMI) {
		static struct ap_power_ev_callback cb;

		ap_power_ev_init_callback(&cb, board_hdmi_handler,
					  AP_POWER_RESUME | AP_POWER_SUSPEND);
		ap_power_ev_add_callback(&cb);
	}
	/* drop related C0 port drivers when it's a HDMI DB. */
	ppc_chips[USBC_PORT_C0] =
		(const struct ppc_config_t){ .drv = &virtual_ppc_drv };
	tcpc_config[USBC_PORT_C0] =
		(const struct tcpc_config_t){ .drv = &virtual_tcpc_drv };

	hook_call_deferred(&tasks_init_deferred_data, 500 * MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_x_ec_gpio2_init, HOOK_PRIO_DEFAULT);

__override uint8_t get_dp_pin_mode(int port)
{
	if (veluza_get_db_type() == VELUZA_DB_HDMI) {
		if (usb_mux_get(USBC_PORT_C0) & USB_PD_MUX_DP_ENABLED) {
			return MODE_DP_PIN_E;
		} else {
			return 0;
		}
	}

	return pd_dfp_dp_get_pin_mode(port, dp_status[port]);
}

void x_ec_interrupt(enum gpio_signal signal)
{
	int sub = veluza_get_db_type();

	if (sub == VELUZA_DB_HDMI) {
		hdmi_hpd_interrupt(signal);
	} else {
		CPRINTS("Undetected subboard interrupt.");
	}
}
