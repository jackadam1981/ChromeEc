/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio/gpio_emul.h>

#include "ec_commands.h"
#include "ec_tasks.h"
#include "emul/emul_isl923x.h"
#include "emul/tcpc/emul_ps8xxx.h"
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_drp.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "host_command.h"
#include "stubs.h"
#include "tcpm/tcpci.h"
#include "test/usb_pe.h"
#include "utils.h"
#include "test_state.h"

struct integration_usb_alt_mode_fixture {
	const struct emul *tcpci_generic_emul;
};

static void *integration_usb_alt_mode_setup(void)
{
	const struct emul *tcpci_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_emul)));

	static struct integration_usb_attach_src_then_snk_fixture emul_state = {
		.tcpci_generic_emul = tcpci_emul,
	};

	return &emul_state;
}

static void integration_usb_alt_mode_before(void *state)
{
	struct integration_usb_alt_mode_fixture *fixture = state;

	const struct emul *tcpci_emul_src = fixture->tcpci_generic_emul;

	struct tcpci_snk_emul ufp_partner;

	zassume_ok(tcpc_config[USBC_PORT_C0].drv->init(USBC_PORT_C0), NULL);
	tcpci_emul_set_rev(tcpci_emul_src, TCPCI_EMUL_REV1_0_VER1_0);
	pd_set_suspend(USBC_PORT_C0, false);
	/* Reset to disconnected state. */
	zassume_ok(tcpci_emul_disconnect_partner(tcpci_emul_src), NULL);
	zassume_ok(tcpci_emul_disconnect_partner(tcpci_emul_snk), NULL);

	/* 1) Attach SOURCE */

	/* Attach emulated charger. */
	tcpci_src_emul_init(&my_charger);
	zassume_ok(tcpci_src_emul_connect_to_tcpci(
			   &my_charger.data, &my_charger.common_data,
			   &my_charger.ops, tcpci_emul_src),
		   NULL);
	isl923x_emul_set_adc_vbus(charger_emul, DEFAULT_VBUS_MV);

	/* Wait for current ramp. */
	k_sleep(K_SECONDS(10));

	/* 2) Attach SINK */

	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	/* Attach emulated sink */
	tcpci_snk_emul_init(&ufp_partner);

	zassume_ok(tcpci_snk_emul_connect_to_tcpci(
			   &ufp_partner.data, &ufp_partner.common_data, &ufp_partner.ops,
			   tcpci_emul_snk),
		   NULL);

	/* Wait for PD negotiation */
	k_sleep(K_SECONDS(10));
}

static void integration_usb_alt_mode_after(void *state)
{
	struct integration_usb_attach_src_then_snk_fixture *my_state = state;

	const struct emul *tcpci_generic_emul = my_state->tcpci_generic_emul;
	const struct emul *tcpci_ps8xxx_emul = my_state->tcpci_ps8xxx_emul;
	const struct emul *charger_emul = my_state->charger_isl923x_emul;

	tcpci_emul_disconnect_partner(tcpci_generic_emul);
	tcpci_emul_disconnect_partner(tcpci_ps8xxx_emul);

	/* Give time to actually disconnect */
	k_sleep(K_SECONDS(1));

	/* Reset vbus to 0mV */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(charger_emul, 0);
}

ZTEST_F(integration_usb_alt_mode, verify_snk_port_pd_info)
{
	struct ec_params_usb_pd_power_info params = { .port = SNK_PORT };
	struct ec_response_usb_pd_power_info response;
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_USB_PD_POWER_INFO, 0, response, params);

	/* Assume */
	zassume_ok(host_command_process(&args), "Failed to get PD power info");

	/* Assert */
	zassert_equal(response.role, USB_PD_PORT_POWER_SINK,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_SINK, response.role);
	zassert_equal(response.type, USB_CHG_TYPE_PD,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_PD, response.type);

	zassert_equal(response.meas.voltage_max, DEFAULT_VBUS_MV,
		      "Charging at VBUS %dmV, but PD reports %dmV",
		      DEFAULT_VBUS_MV, response.meas.voltage_max);

	zassert_within(response.meas.voltage_now, DEFAULT_VBUS_MV,
		       DEFAULT_VBUS_MV / 10,
		       "Actually charging at VBUS %dmV, but PD reports %dmV",
		       DEFAULT_VBUS_MV, response.meas.voltage_now);

	zassert_equal(response.meas.current_max, DEFAULT_VBUS_MA,
		      "Charging at VBUS max %dmA, but PD reports %dmA",
		      DEFAULT_VBUS_MA, response.meas.current_max);

	zassert_true(response.meas.current_lim >= DEFAULT_VBUS_MA,
		     "Charging at VBUS max %dmA, but PD current limit %dmA",
		     DEFAULT_VBUS_MA, response.meas.current_lim);

	zassert_equal(response.max_power, DEFAULT_VBUS_MV * DEFAULT_VBUS_MA,
		      "Charging up to %duW, PD max power %duW",
		      DEFAULT_VBUS_MV * DEFAULT_VBUS_MA, response.max_power);
}

ZTEST_SUITE(integration_usb_alt_mode, NULL,
	    integration_usb_alt_mode_setup,
	    integration_usb_alt_mode_before,
	    integration_usb_alt_mode_after, NULL);
