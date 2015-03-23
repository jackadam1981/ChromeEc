/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* oak USB-C port voltage control */

#include "config.h"
#include "console.h"
#include "driver/pi3usb30532.h"
#include "driver/pi3usb9281.h"
#include "ec_commands.h"
#include "gpio.h"
#include "host_command.h"
#include "task.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* USB-C 5V output enable pins */
static const unsigned usbc_5v_out_gpios[] = {GPIO_USB_C0_5V_OUT,
					     GPIO_USB_C1_5V_OUT};
/* USB-C charge enable pins */
static const unsigned usbc_charge_gpios[] = {GPIO_USB_C0_CHARGE_L,
					     GPIO_USB_C1_CHARGE_L};

static struct {
	uint8_t mux;
	uint8_t polarity;
	uint8_t power_role;
	uint8_t data_role;
	uint8_t dp_flags;
} pd_ports[PD_PORT_COUNT];

/* VBUS wake proxy */
void vbus_wake_interrupt(enum gpio_signal signal)
{
	gpio_set_level(GPIO_USB_PD_VBUS_WAKE,
		       !gpio_get_level(GPIO_VBUS_WAKE_L));
}

static void set_charge_port(int charge_port)
{
	int port;

	if (charge_port < 0) {
		/* Disable both charge port */
		gpio_set_level(GPIO_USB_C0_CHARGE_L, 1);
		gpio_set_level(GPIO_USB_C1_CHARGE_L, 1);
		return;
	}

	for (port = 0; port < PD_PORT_COUNT; port++) {
		if (port == charge_port) {
			/* Disable 5V output */
			gpio_set_level(usbc_5v_out_gpios[port], 0);
			/* Enable charge port */
			gpio_set_level(usbc_charge_gpios[port], 0);
		} else {
			gpio_set_level(usbc_charge_gpios[port], 1);
			/* Enable 5V output on switching to power source role */
			if (pd_ports[port].data_role == PD_ROLE_DFP &&
			    pd_ports[port].power_role != PD_ROLE_SINK)
				gpio_set_level(usbc_5v_out_gpios[port], 1);
			else
				gpio_set_level(usbc_5v_out_gpios[port], 0);
		}
	}
}

static int set_usbc_switch(int port, int mux, int polarity)
{
	const uint8_t mux_modes[] = {
		[TYPEC_MUX_NONE] = PI3USB30532_MODE_POWERDOWN,
		[TYPEC_MUX_USB] = PI3USB30532_MODE_USB,
		[TYPEC_MUX_DP] = PI3USB30532_MODE_DP,
		[TYPEC_MUX_DOCK] = PI3USB30532_MODE_DP_USB
	};
	uint8_t mode;

	mode = mux_modes[mux];
	if (polarity && mux != TYPEC_MUX_NONE)
		mode |= PI3USB30532_BIT_SWAP;

	return pi3usb30532_set_switch(port, mode);
}

/* TODO: VBUS wake interrupt */
/* gpio_set_level(GPIO_USB_PD_VBUS_WAKE, 0); */

static int charge_port = -PD_PORT_COUNT;

void board_sync_usbc_port()
{
	struct ec_params_pd_port params_pd_port;
	struct ec_response_pd_port port_status;
	int port, active_charge_port;
	int rv;
	/* Configure charge port */
	active_charge_port = pd_get_active_charge_port();
	if (charge_port != active_charge_port) {
		charge_port = active_charge_port;
		set_charge_port(charge_port);
	}
	/* Configure USB-C mux */
	for (port = 0; port < PD_PORT_COUNT; port++) {
		/* Get PD port status from PD MCU */
		params_pd_port.port = port;
		rv = pd_host_command(EC_CMD_PD_GET_PORT_STATUS, 0,
				     &params_pd_port,
				     sizeof(params_pd_port),
				     &port_status,
				     sizeof(port_status));
		if (rv) {
			CPRINTS("Host command to PD MCU failed");
			continue;
		}

		/* Set USB-C switch if mux or polarity changed */
		if (port_status.mux != pd_ports[port].mux ||
		    port_status.polarity != pd_ports[port].polarity) {
			pd_ports[port].mux = port_status.mux;
			pd_ports[port].polarity = port_status.polarity;
			rv = set_usbc_switch(port, port_status.mux,
					     port_status.polarity);
			if (rv)
				CPRINTS("Set USBC mux failed");

		}
	}
}
