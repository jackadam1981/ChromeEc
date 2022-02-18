/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio/gpio_emul.h>

#include "ec_commands.h"
#include "ec_tasks.h"
#include "driver/tcpm/ps8xxx_public.h"
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
#include "test_state.h"
#include "usb_integration.h"
#include "utils.h"

#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_emul)
#define TCPCI_PS8XXX_EMUL_LABEL DT_NODELABEL(tcpci_ps8xxx_emul)

#define DEFAULT_VBUS_MV 5000
#define DEFAULT_VBUS_MA 3000

static void integration_usb_detach(const struct emul *e)
{
	zassume_ok(tcpci_emul_disconnect_partner(e), NULL);
}

static void integration_usb_init(const struct emul *e, int port)
{
	zassume_ok(tcpc_config[port].drv->init(port), NULL);

	if (port == USB_SINK_PORT) {
		tcpci_emul_set_reg(e, PS8XXX_REG_FW_REV, 0x31);
	} else {
		tcpci_emul_set_rev(e, TCPCI_EMUL_REV1_0_VER1_0);
	}
	pd_set_suspend(port, 0);

	integration_usb_detach(e);
}

void *integration_usb_setup(void)
{
	const struct emul *tcpci_emul =
		emul_get_binding(DT_LABEL(TCPCI_EMUL_LABEL));
	const struct emul *ps8xxx_emul =
		emul_get_binding(DT_LABEL(TCPCI_PS8XXX_EMUL_LABEL));
	const struct emul *charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	static struct integration_usb_fixture emul_state;

	emul_state.source = tcpci_emul;
	emul_state.sink = ps8xxx_emul;
	emul_state.charger = charger_emul;

	return &emul_state;
}


void integration_usb_sink_attach(struct integration_usb_fixture *fixture)
{
	struct tcpci_snk_emul my_sink;

	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	/* Attach emulated sink */
	tcpci_snk_emul_init(&my_sink);

	zassume_ok(tcpci_snk_emul_connect_to_tcpci(&my_sink.data,
						   &my_sink.common_data,
						   &my_sink.ops, fixture->sink),
		   NULL);

	/* Wait for PD negotiation */
	k_sleep(K_SECONDS(10));
}

void integration_usb_sink_detach(struct integration_usb_fixture *fixture)
{
	integration_usb_detach(fixture->sink);
}

void integration_usb_source_attach(struct integration_usb_fixture *fixture)
{
	struct tcpci_src_emul my_charger;

	/* Attach emulated charger. */
	tcpci_src_emul_init(&my_charger);
	zassume_ok(tcpci_src_emul_connect_to_tcpci(
			   &my_charger.data, &my_charger.common_data,
			   &my_charger.ops, fixture->source),
		   NULL);
	isl923x_emul_set_adc_vbus(fixture->charger, DEFAULT_VBUS_MV);

	/* Wait for current ramp. */
	k_sleep(K_SECONDS(10));
}

void integration_usb_source_detach(struct integration_usb_fixture *fixture)
{
	integration_usb_detach(fixture->source);
}

void integration_usb_reset(struct integration_usb_fixture *fixture)
{
	/* Reset vbus to 0mV */
	/* TODO(b/217610871): Remove redundant test state cleanup */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	integration_usb_init(fixture->source, USB_SOURCE_PORT);
	integration_usb_init(fixture->sink, USB_SINK_PORT);
}


void integration_usb_get_pd_power_info(
	int port, struct ec_response_usb_pd_power_info *response)
{
	struct ec_params_usb_pd_power_info params = { .port = port };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_USB_PD_POWER_INFO, 0, (*response), params);

	/* Assume */
	zassume_ok(host_command_process(&args), "Failed to get PD power info");
}
