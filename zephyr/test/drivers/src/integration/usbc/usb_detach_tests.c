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
#include "utils.h"

#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_emul)
#define TCPCI_PS8XXX_EMUL_LABEL DT_NODELABEL(tcpci_ps8xxx_emul)

#define USB_SINK_PORT USBC_PORT_C0
#define USB_SOURCE_PORT USBC_PORT_C1

#define DEFAULT_VBUS_MV 5000
#define DEFAULT_VBUS_MA 3000

struct usb_detach_test_fixture {
	const struct emul *source;
	const struct emul *sink;
	const struct emul *charger;
	struct tcpci_src_emul source_emul;
	struct tcpci_snk_emul sink_emul;
};

static struct usb_detach_test_fixture usb_detach_fixture = { 0 };

static void usb_detach_test_detach(const struct emul *e)
{
	zassume_ok(tcpci_emul_disconnect_partner(e), NULL);
}

static void usb_detach_test_init(const struct emul *e, int port)
{
	zassume_ok(tcpc_config[port].drv->init(port), NULL);

	/* TODO: This should be taken care of in the emulator init */
	if (port == USB_SINK_PORT) {
		tcpci_emul_set_reg(e, PS8XXX_REG_FW_REV, 0x31);
	} else {
		tcpci_emul_set_rev(e, TCPCI_EMUL_REV1_0_VER1_0);
	}
	pd_set_suspend(port, 0);

	usb_detach_test_detach(e);
}

static void usb_detach_test_reset(struct usb_detach_test_fixture *fixture)
{
	/* Reset vbus to 0mV */
	/* TODO(b/217610871): Remove redundant test state cleanup */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	usb_detach_test_init(fixture->source, USB_SOURCE_PORT);
	usb_detach_test_init(fixture->sink, USB_SINK_PORT);
}

static void usb_detach_test_sink_attach(struct usb_detach_test_fixture *fixture)
{
	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	/* Attach emulated sink */
	tcpci_snk_emul_init(&fixture->sink_emul);

	zassume_ok(tcpci_snk_emul_connect_to_tcpci(
			   &fixture->sink_emul.data,
			   &fixture->sink_emul.common_data,
			   &fixture->sink_emul.ops, fixture->sink),
		   NULL);

	/* Wait for PD negotiation */
	k_sleep(K_SECONDS(10));
}

void usb_detach_test_sink_detach(struct usb_detach_test_fixture *fixture)
{
	usb_detach_test_detach(fixture->sink);
}

void usb_detach_test_source_attach(struct usb_detach_test_fixture *fixture)
{
	/* Attach emulated charger. */
	tcpci_src_emul_init(&fixture->source_emul);
	zassume_ok(tcpci_src_emul_connect_to_tcpci(
			   &fixture->source_emul.data,
			   &fixture->source_emul.common_data,
			   &fixture->source_emul.ops, fixture->source),
		   NULL);
	isl923x_emul_set_adc_vbus(fixture->charger, DEFAULT_VBUS_MV);

	/* Wait for current ramp. */
	k_sleep(K_SECONDS(10));
}

void usb_detach_test_source_detach(struct usb_detach_test_fixture *fixture)
{
	usb_detach_test_detach(fixture->source);
}

void usb_detach_test_get_pd_power_info(
	int port, struct ec_response_usb_pd_power_info *response)
{
	struct ec_params_usb_pd_power_info params = { .port = port };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_USB_PD_POWER_INFO, 0, (*response), params);

	/* Assume */
	zassume_ok(host_command_process(&args), "Failed to get PD power info");
}

void *usb_detach_test_setup(void)
{
	const struct emul *tcpci_emul =
		emul_get_binding(DT_LABEL(TCPCI_EMUL_LABEL));
	const struct emul *ps8xxx_emul =
		emul_get_binding(DT_LABEL(TCPCI_PS8XXX_EMUL_LABEL));
	const struct emul *charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	usb_detach_fixture.source = tcpci_emul;
	usb_detach_fixture.sink = ps8xxx_emul;
	usb_detach_fixture.charger = charger_emul;

	return &usb_detach_fixture;
}

static void usb_detach_test_before(void *state)
{
	struct usb_detach_test_fixture *fixture = state;

	usb_detach_test_reset(fixture);

	usb_detach_test_sink_attach(fixture);

	usb_detach_test_source_attach(fixture);
}

static void usb_detach_test_after(void *state)
{
	struct usb_detach_test_fixture *fixture = state;

	usb_detach_test_source_detach(fixture);
	usb_detach_test_sink_detach(fixture);

	/* Give time to actually disconnect */
	k_sleep(K_SECONDS(1));

	/* Reset vbus to 0mV */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(fixture->charger, 0);
}

ZTEST_F(usb_detach_test, verify_detach_src_snk)
{
	struct usb_detach_test_fixture *fixture = this;
	struct ec_response_usb_pd_power_info src_power_info = { 0 };
	struct ec_response_usb_pd_power_info snk_power_info = { 0 };

	usb_detach_test_source_detach(fixture);
	usb_detach_test_sink_detach(fixture);

	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	usb_detach_test_get_pd_power_info(USB_SINK_PORT, &snk_power_info);
	usb_detach_test_get_pd_power_info(USB_SOURCE_PORT, &src_power_info);

	/* Validate Sink power info */
	zassert_equal(snk_power_info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_DISCONNECTED, snk_power_info.role);
	zassert_equal(snk_power_info.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, snk_power_info.type);

	zassert_equal(snk_power_info.meas.voltage_max, 0,
		      "Charging at VBUS %dmV, but PD reports %dmV", 0,
		      snk_power_info.meas.voltage_max);

	zassert_within(snk_power_info.meas.voltage_now, 0, 10,
		       "Actually charging at VBUS %dmV, but PD reports %dmV", 0,
		       snk_power_info.meas.voltage_now);

	zassert_equal(snk_power_info.meas.current_max, 0,
		      "Charging at VBUS max %dmA, but PD reports %dmA", 0,
		      snk_power_info.meas.current_max);

	zassert_true(snk_power_info.meas.current_lim >= 0,
		     "Charging at VBUS max %dmA, but PD current limit %dmA", 0,
		     snk_power_info.meas.current_lim);

	zassert_equal(snk_power_info.max_power, 0,
		      "Charging up to %duW, PD max power %duW", 0,
		      snk_power_info.max_power);

	/* Validate Source power info */
	zassert_equal(src_power_info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_DISCONNECTED, src_power_info.role);

	zassert_equal(src_power_info.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, src_power_info.type);

	/* TODO(b/209907615): Confirm measure value requirements */
	zassert_within(src_power_info.meas.voltage_now, 0, 10,
		       "Expected Charging at VBUS %dmV, but PD reports %dmV",
		       DEFAULT_VBUS_MV, src_power_info.meas.voltage_now);

	zassume_equal(src_power_info.meas.current_max, 0,
		      "Charging at VBUS max %dmA, but PD reports %dmA", 0,
		      src_power_info.meas.current_max);
}

ZTEST_F(usb_detach_test, verify_detach_snk_src)
{
	struct usb_detach_test_fixture *fixture = this;
	struct ec_response_usb_pd_power_info src_power_info = { 0 };
	struct ec_response_usb_pd_power_info snk_power_info = { 0 };

	usb_detach_test_sink_detach(fixture);
	usb_detach_test_source_detach(fixture);

	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	usb_detach_test_get_pd_power_info(USB_SINK_PORT, &snk_power_info);
	usb_detach_test_get_pd_power_info(USB_SOURCE_PORT, &src_power_info);

	/* Validate Sink power info */
	zassert_equal(snk_power_info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_DISCONNECTED, snk_power_info.role);
	zassert_equal(snk_power_info.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, snk_power_info.type);

	zassert_equal(snk_power_info.meas.voltage_max, 0,
		      "Charging at VBUS %dmV, but PD reports %dmV", 0,
		      snk_power_info.meas.voltage_max);

	zassert_within(snk_power_info.meas.voltage_now, 0, 10,
		       "Actually charging at VBUS %dmV, but PD reports %dmV", 0,
		       snk_power_info.meas.voltage_now);

	zassert_equal(snk_power_info.meas.current_max, 0,
		      "Charging at VBUS max %dmA, but PD reports %dmA", 0,
		      snk_power_info.meas.current_max);

	zassert_true(snk_power_info.meas.current_lim >= 0,
		     "Charging at VBUS max %dmA, but PD current limit %dmA", 0,
		     snk_power_info.meas.current_lim);

	zassert_equal(snk_power_info.max_power, 0,
		      "Charging up to %duW, PD max power %duW", 0,
		      snk_power_info.max_power);

	/* Validate Source power info */
	zassert_equal(src_power_info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_DISCONNECTED, src_power_info.role);

	zassert_equal(src_power_info.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, src_power_info.type);

	/* TODO(b/209907615): Confirm measure value requirements */
	zassert_within(src_power_info.meas.voltage_now, 0, 10,
		       "Expected Charging at VBUS %dmV, but PD reports %dmV",
		       DEFAULT_VBUS_MV, src_power_info.meas.voltage_now);

	zassume_equal(src_power_info.meas.current_max, 0,
		      "Charging at VBUS max %dmA, but PD reports %dmA", 0,
		      src_power_info.meas.current_max);
}

ZTEST_F(usb_detach_test, verify_detach_sink)
{
	struct usb_detach_test_fixture *fixture = this;
	struct ec_response_usb_pd_power_info pd_power_info = { 0 };

	usb_detach_test_sink_detach(fixture);
	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	usb_detach_test_get_pd_power_info(USB_SINK_PORT, &pd_power_info);

	/* Assert */
	zassert_equal(pd_power_info.role, USB_PD_PORT_POWER_SINK,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_SINK, pd_power_info.role);
	zassert_equal(pd_power_info.type, USB_CHG_TYPE_VBUS,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_VBUS, pd_power_info.type);

	zassert_equal(pd_power_info.meas.voltage_max, DEFAULT_VBUS_MV,
		      "Charging at VBUS %dmV, but PD reports %dmV",
		      DEFAULT_VBUS_MV, pd_power_info.meas.voltage_max);

	zassert_within(pd_power_info.meas.voltage_now, 0, 10,
		       "Actually charging at VBUS %dmV, but PD reports %dmV", 0,
		       pd_power_info.meas.voltage_now);

	zassert_equal(pd_power_info.meas.current_max, 500,
		      "Charging at VBUS max %dmA, but PD reports %dmA",
		      500, pd_power_info.meas.current_max);

	zassert_true(pd_power_info.meas.current_lim >= 500,
		     "Charging at VBUS max %dmA, but PD current limit %dmA",
		     500, pd_power_info.meas.current_lim);

	zassert_equal(pd_power_info.max_power,
		      2500000,
		      "Charging up to %duW, PD max power %duW",
		      2500000,
		      pd_power_info.max_power);
}

ZTEST_F(usb_detach_test, verify_detach_source)
{
	struct usb_detach_test_fixture *fixture = this;
	struct ec_response_usb_pd_power_info pd_power_info = { 0 };

	usb_detach_test_source_detach(fixture);
	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	usb_detach_test_get_pd_power_info(USB_SOURCE_PORT, &pd_power_info);

	/* Assert */
	zassert_equal(pd_power_info.role, USB_PD_PORT_POWER_DISCONNECTED,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_DISCONNECTED, pd_power_info.role);

	zassert_equal(pd_power_info.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, pd_power_info.type);

	/* TODO(b/209907615): Confirm measure value requirements */
	zassert_within(pd_power_info.meas.voltage_now, 0, 10,
		       "Expected Charging at VBUS %dmV, but PD reports %dmV",
		       DEFAULT_VBUS_MV, pd_power_info.meas.voltage_now);

	zassume_equal(pd_power_info.meas.current_max, 0,
		      "Charging at VBUS max %dmA, but PD reports %dmA", 0,
		      pd_power_info.meas.current_max);
}

ZTEST_SUITE(usb_detach_test, drivers_predicate_post_main, usb_detach_test_setup,
	    usb_detach_test_before, usb_detach_test_after, NULL);
