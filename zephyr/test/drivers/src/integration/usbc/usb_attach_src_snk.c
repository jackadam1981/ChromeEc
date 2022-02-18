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
#include "utils.h"
#include "test_state.h"

#define SNK_PORT USBC_PORT_C0
#define SRC_PORT USBC_PORT_C1

#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_emul)
#define TCPCI_PS8XXX_EMUL_LABEL DT_NODELABEL(tcpci_ps8xxx_emul)

#define DEFAULT_VBUS_MV 5000
#define DEFAULT_VBUS_MA 3000

struct integration_usb_attach_src_then_snk_fixture {
	/* TODO(b/217737667): Remove driver specific code. */
	const struct emul *tcpci_generic_emul;
	const struct emul *tcpci_ps8xxx_emul;
	const struct emul *charger_isl923x_emul;
	struct tcpci_src_emul my_src;
	struct tcpci_snk_emul my_snk;
};

struct integration_usb_attach_snk_then_src_fixture {
	/* TODO(b/217737667): Remove driver specific code. */
	const struct emul *tcpci_generic_emul;
	const struct emul *tcpci_ps8xxx_emul;
	const struct emul *charger_isl923x_emul;
	struct tcpci_src_emul my_src;
	struct tcpci_snk_emul my_snk;
};

struct integration_usb_test_fixture {
	const struct emul *source;
	const struct emul *sink;
	const struct emul *charger;
	struct tcpci_src_emul source_emul;
	struct tcpci_snk_emul sink_emul;
};

static void integration_usb_test_detach(const struct emul *e)
{
	zassume_ok(tcpci_emul_disconnect_partner(e), NULL);
}

static void integration_usb_test_init(const struct emul *e, int port)
{
	zassume_ok(tcpc_config[port].drv->init(port), NULL);

	/* TODO: This should be taken care of in the emulator init */
	if (port == SNK_PORT) {
		tcpci_emul_set_reg(e, PS8XXX_REG_FW_REV, 0x31);
	} else {
		tcpci_emul_set_rev(e, TCPCI_EMUL_REV1_0_VER1_0);
	}
	pd_set_suspend(port, 0);

	integration_usb_test_detach(e);
}

static void
integration_usb_test_reset(struct integration_usb_test_fixture *fixture)
{
	/* Reset vbus to 0mV */
	/* TODO(b/217610871): Remove redundant test state cleanup */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	integration_usb_test_init(fixture->source, SRC_PORT);
	integration_usb_test_init(fixture->sink, SNK_PORT);
}

static void
integration_usb_test_sink_attach(struct integration_usb_test_fixture *fixture)
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

static void
integration_usb_test_sink_detach(struct integration_usb_test_fixture *fixture)
{
	integration_usb_test_detach(fixture->sink);
}

static void
integration_usb_test_source_attach(struct integration_usb_test_fixture *fixture)
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

static void
integration_usb_test_source_detach(struct integration_usb_test_fixture *fixture)
{
	integration_usb_test_detach(fixture->source);
}

static void *integration_usb_src_snk_setup(void)
{
	const struct emul *tcpci_emul =
		emul_get_binding(DT_LABEL(TCPCI_EMUL_LABEL));
	const struct emul *tcpci_emul2 =
		emul_get_binding(DT_LABEL(TCPCI_PS8XXX_EMUL_LABEL));
	const struct emul *charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));
	static struct tcpci_src_emul my_src;
	static struct tcpci_snk_emul my_snk;

	static struct integration_usb_attach_src_then_snk_fixture emul_state;

	/* Setting these are required because compiler believes these values are
	 * not compile time constants.
	 */
	/*
	 * TODO(b/217758708): emuls should be identified at compile-time.
	 */
	emul_state.tcpci_generic_emul = tcpci_emul;
	emul_state.tcpci_ps8xxx_emul = tcpci_emul2;
	emul_state.charger_isl923x_emul = charger_emul;
	emul_state.my_src = my_src;
	emul_state.my_snk = my_snk;

	return &emul_state;
}

static void integration_usb_attach_snk_then_src_before(void *state)
{
	const struct integration_usb_attach_src_then_snk_fixture *my_state =
		state;
	const struct emul *tcpci_emul_src = my_state->tcpci_generic_emul;
	const struct emul *tcpci_emul_snk = my_state->tcpci_ps8xxx_emul;
	const struct emul *charger_emul = my_state->charger_isl923x_emul;
	struct tcpci_src_emul my_src = my_state->my_src;
	struct tcpci_snk_emul my_snk = my_state->my_snk;

	/* Reset vbus to 0mV */
	/* TODO(b/217610871): Remove redundant test state cleanup */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(charger_emul, 0);

	zassume_ok(tcpc_config[SNK_PORT].drv->init(SNK_PORT), NULL);
	/*
	 * Arbitrary FW ver. The emulator should really be setting this
	 * during its init.
	 */
	tcpci_emul_set_reg(tcpci_emul_snk, PS8XXX_REG_FW_REV, 0x31);
	zassume_ok(tcpc_config[SRC_PORT].drv->init(SRC_PORT), NULL);
	tcpci_emul_set_rev(tcpci_emul_src, TCPCI_EMUL_REV1_0_VER1_0);
	pd_set_suspend(SNK_PORT, 0);
	pd_set_suspend(SRC_PORT, 0);
	/* Reset to disconnected state. */
	zassume_ok(tcpci_emul_disconnect_partner(tcpci_emul_src), NULL);
	zassume_ok(tcpci_emul_disconnect_partner(tcpci_emul_snk), NULL);

	/* 1) Attach SINK */

	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	/* Attach emulated sink */
	tcpci_snk_emul_init(&my_snk);

	zassume_ok(tcpci_snk_emul_connect_to_tcpci(
			   &my_snk.data, &my_snk.common_data, &my_snk.ops,
			   tcpci_emul_snk),
		   NULL);

	/* Wait for PD negotiation */
	k_sleep(K_SECONDS(10));

	/* 2) Attach SOURCE */

	/* Attach emulated charger. */
	tcpci_src_emul_init(&my_src);
	zassume_ok(tcpci_src_emul_connect_to_tcpci(
			   &my_src.data, &my_src.common_data, &my_src.ops,
			   tcpci_emul_src),
		   NULL);
	isl923x_emul_set_adc_vbus(charger_emul, DEFAULT_VBUS_MV);

	/* Wait for current ramp. */
	k_sleep(K_SECONDS(10));
}

static void integration_usb_attach_src_then_snk_before(void *state)
{
	struct integration_usb_attach_src_then_snk_fixture *my_state = state;

	const struct emul *tcpci_emul_src = my_state->tcpci_generic_emul;
	const struct emul *tcpci_emul_snk = my_state->tcpci_ps8xxx_emul;
	const struct emul *charger_emul = my_state->charger_isl923x_emul;

	struct tcpci_src_emul *my_src = &my_state->my_src;
	struct tcpci_snk_emul *my_snk = &my_state->my_snk;

	/* Reset vbus to 0mV */
	/* TODO(b/217610871): Remove redundant test state cleanup */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(charger_emul, 0);

	zassume_ok(tcpc_config[SNK_PORT].drv->init(SNK_PORT), NULL);
	/*
	 * Arbitrary FW ver. The emulator should really be setting this
	 * during its init.
	 */
	tcpci_emul_set_reg(tcpci_emul_snk, PS8XXX_REG_FW_REV, 0x31);
	zassume_ok(tcpc_config[SRC_PORT].drv->init(SRC_PORT), NULL);
	tcpci_emul_set_rev(tcpci_emul_src, TCPCI_EMUL_REV1_0_VER1_0);
	pd_set_suspend(SNK_PORT, false);
	pd_set_suspend(SRC_PORT, false);
	/* Reset to disconnected state. */
	zassume_ok(tcpci_emul_disconnect_partner(tcpci_emul_src), NULL);
	zassume_ok(tcpci_emul_disconnect_partner(tcpci_emul_snk), NULL);

	/* 1) Attach SOURCE */

	/* Attach emulated charger. */
	tcpci_src_emul_init(my_src);
	zassume_ok(tcpci_src_emul_connect_to_tcpci(
			   &my_src->data, &my_snk->common_data, &my_snk->ops,
			   tcpci_emul_src),
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
	tcpci_snk_emul_init(my_snk);

	zassume_ok(tcpci_snk_emul_connect_to_tcpci(
			   &my_snk->data, &my_snk->common_data, &my_snk->ops,
			   tcpci_emul_snk),
		   NULL);

	/* Wait for PD negotiation */
	k_sleep(K_SECONDS(10));
}

static void integration_usb_attach_src_snk_after(void *state)
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

ZTEST_F(integration_usb_attach_src_then_snk, verify_snk_port_pd_info)
{
	struct ec_response_usb_pd_power_info response;

	response = host_cmd_power_info(SNK_PORT);

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

ZTEST_F(integration_usb_attach_src_then_snk, verify_src_port_pd_info)
{
	struct ec_response_usb_pd_power_info response;

	response = host_cmd_power_info(SRC_PORT);

	/* Assert */
	zassert_equal(response.role, USB_PD_PORT_POWER_SOURCE,
		      "Power role %d, but PD reports role %d", PD_ROLE_SOURCE,
		      response.role);

	zassert_equal(response.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, response.type);

	/* TODO(b/209907615): Confirm measure value requirements */
	zassert_within(response.meas.voltage_now, DEFAULT_VBUS_MV,
		       DEFAULT_VBUS_MV / 10,
		       "Expected Charging at VBUS %dmV, but PD reports %dmV",
		       DEFAULT_VBUS_MV, response.meas.voltage_now);

	zassume_equal(response.meas.current_max, DEFAULT_VBUS_MA,
		      "Charging at VBUS max %dmA, but PD reports %dmA",
		      DEFAULT_VBUS_MA, response.meas.current_max);

	/* Note: We are the source so we skip checking: */
	/* meas.voltage_max */
	/* max_power */
	/* current limit */
}

ZTEST_F(integration_usb_attach_snk_then_src, verify_snk_port_pd_info)
{
	struct ec_response_usb_pd_power_info response;

	response = host_cmd_power_info(SNK_PORT);

	/* Assert */
	zassert_equal(response.role, USB_PD_PORT_POWER_SINK,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_SINK, response.role);
	zassert_equal(response.type, USB_CHG_TYPE_PD,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_PD, response.type);

	/* Verify Default 5V and 3A */
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

ZTEST_F(integration_usb_attach_snk_then_src, verify_src_port_pd_info)
{
	struct ec_response_usb_pd_power_info response;

	response = host_cmd_power_info(SRC_PORT);

	/* Assert */
	zassert_equal(response.role, USB_PD_PORT_POWER_SOURCE,
		      "Power role %d, but PD reports role %d", PD_ROLE_SOURCE,
		      response.role);

	zassert_equal(response.type, USB_CHG_TYPE_NONE,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_NONE, response.type);

	/* Verify Default 5V and 3A */
	/* TODO(b/209907615): Confirm measure value requirements */
	zassert_within(response.meas.voltage_now, DEFAULT_VBUS_MV,
		       DEFAULT_VBUS_MV / 10,
		       "Expected Charging at VBUS %dmV, but PD reports %dmV",
		       DEFAULT_VBUS_MV, response.meas.voltage_now);

	zassume_equal(response.meas.current_max, DEFAULT_VBUS_MA,
		      "Charging at VBUS max %dmA, but PD reports %dmA",
		      DEFAULT_VBUS_MA, response.meas.current_max);

	/* Note: We are the source so we skip checking: */
	/* meas.voltage_max */
	/* max_power */
	/* current limit */
}

ZTEST_SUITE(integration_usb_attach_src_then_snk, drivers_predicate_post_main,
	    integration_usb_src_snk_setup,
	    integration_usb_attach_src_then_snk_before,
	    integration_usb_attach_src_snk_after, NULL);

ZTEST_SUITE(integration_usb_attach_snk_then_src, drivers_predicate_post_main,
	    integration_usb_src_snk_setup,
	    integration_usb_attach_snk_then_src_before,
	    integration_usb_attach_src_snk_after, NULL);

struct usb_detach_test_fixture {
	struct integration_usb_test_fixture fixture;
};

void *usb_detach_test_setup(void)
{
	const struct emul *tcpci_emul =
		emul_get_binding(DT_LABEL(TCPCI_EMUL_LABEL));
	const struct emul *ps8xxx_emul =
		emul_get_binding(DT_LABEL(TCPCI_PS8XXX_EMUL_LABEL));
	const struct emul *charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	static struct usb_detach_test_fixture usb_detach_fixture = { 0 };

	usb_detach_fixture.fixture.source = tcpci_emul;
	usb_detach_fixture.fixture.sink = ps8xxx_emul;
	usb_detach_fixture.fixture.charger = charger_emul;

	return &usb_detach_fixture;
}

static void usb_detach_test_before(void *state)
{
	struct integration_usb_test_fixture *fixture = state;

	integration_usb_test_reset(fixture);

	integration_usb_test_sink_attach(fixture);

	integration_usb_test_source_attach(fixture);
}

static void usb_detach_test_after(void *state)
{
	struct integration_usb_test_fixture *fixture = state;

	integration_usb_test_source_detach(fixture);
	integration_usb_test_sink_detach(fixture);

	/* Give time to actually disconnect */
	k_sleep(K_SECONDS(1));

	/* Reset vbus to 0mV */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(fixture->charger, 0);
}

ZTEST_F(usb_detach_test, verify_detach_src_snk)
{
	struct integration_usb_test_fixture *fixture = &this->fixture;
	struct ec_response_usb_pd_power_info src_power_info = { 0 };
	struct ec_response_usb_pd_power_info snk_power_info = { 0 };

	integration_usb_test_source_detach(fixture);
	integration_usb_test_sink_detach(fixture);

	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	snk_power_info = host_cmd_power_info(SNK_PORT);
	src_power_info = host_cmd_power_info(SRC_PORT);

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
	struct integration_usb_test_fixture *fixture = &this->fixture;
	struct ec_response_usb_pd_power_info src_power_info = { 0 };
	struct ec_response_usb_pd_power_info snk_power_info = { 0 };

	integration_usb_test_sink_detach(fixture);
	integration_usb_test_source_detach(fixture);

	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	snk_power_info = host_cmd_power_info(SNK_PORT);
	src_power_info = host_cmd_power_info(SRC_PORT);

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
	struct integration_usb_test_fixture *fixture = &this->fixture;
	struct ec_response_usb_pd_power_info pd_power_info = { 0 };

	integration_usb_test_sink_detach(fixture);
	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	pd_power_info = host_cmd_power_info(SNK_PORT);

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
		      "Charging at VBUS max %dmA, but PD reports %dmA", 500,
		      pd_power_info.meas.current_max);

	zassert_true(pd_power_info.meas.current_lim >= 500,
		     "Charging at VBUS max %dmA, but PD current limit %dmA",
		     500, pd_power_info.meas.current_lim);

	zassert_equal(pd_power_info.max_power, 2500000,
		      "Charging up to %duW, PD max power %duW", 2500000,
		      pd_power_info.max_power);
}

ZTEST_F(usb_detach_test, verify_detach_source)
{
	struct integration_usb_test_fixture *fixture = &this->fixture;
	struct ec_response_usb_pd_power_info pd_power_info = { 0 };

	integration_usb_test_source_detach(fixture);
	k_sleep(K_SECONDS(10));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	pd_power_info = host_cmd_power_info(SRC_PORT);

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

ZTEST_SUITE(usb_detach_test, drivers_predicate_post_main,
	    integration_usb_src_snk_setup, usb_detach_test_before,
	    usb_detach_test_after, NULL);
