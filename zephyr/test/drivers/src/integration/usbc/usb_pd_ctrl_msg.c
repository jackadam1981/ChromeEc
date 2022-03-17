/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <ztest.h>

#include "battery_smart.h"
#include "common.h"
#include "ec_tasks.h"
#include "emul/emul_isl923x.h"
#include "emul/emul_smart_battery.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "emul/tcpc/emul_tcpci_partner_drp.h"
#include "stubs.h"
#include "tcpm/tcpci.h"
#include "test_state.h"
#include "utils.h"
#include "usb_pd.h"

#define SRC_PORT USBC_PORT_C0
#define SNK_PORT USBC_PORT_C1

#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_emul)
#define TCPCI_PS8XXX_EMUL_LABEL DT_NODELABEL(tcpci_ps8xxx_emul)

#define DEFAULT_VBUS_MV 5000

/* Determined by CONFIG_PLATFORM_EC_USB_PD_PULLUP */
#define DEFAULT_VBUS_SRC_PORT_MA 1500

/* SRC TCPCI Emulator attaches as TYPEC_CC_VOLT_RP_3_0 */
#define DEFAULT_VBUS_SNK_PORT_MA 3000

#define DEFAULT_SINK_SENT_TO_SOURCE_CAP_COUNT 1
#define DEFAULT_SOURCE_SENT_TO_SINK_CAP_COUNT 1

struct usb_pd_ctrl_msg_test_fixture {
	struct tcpci_drp_emul sink_5v_3a;
	struct tcpci_drp_emul source_5v_3a;
	const struct emul *tcpci_snk_emul;
	const struct emul *tcpci_src_emul;
	const struct emul *charger_emul;
};

static void connect_sink_to_port(struct usb_pd_ctrl_msg_test_fixture *fixture)
{
	/*
	 * TODO(b/221439302) Updating the TCPCI emulator registers, updating the
	 *   vbus, as well as alerting should all be a part of the connect
	 *   function.
	 */
	isl923x_emul_set_adc_vbus(fixture->charger_emul, 0);
	tcpci_emul_set_reg(fixture->tcpci_snk_emul, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_VBUS_DET);
	tcpci_emul_set_reg(fixture->tcpci_snk_emul, TCPC_REG_EXT_STATUS,
			   TCPC_REG_EXT_STATUS_SAFE0V);
	tcpci_tcpc_alert(SNK_PORT);
	zassume_ok(tcpci_drp_emul_connect_to_tcpci(
			   &fixture->sink_5v_3a.data,
			   &fixture->sink_5v_3a.src_data,
			   &fixture->sink_5v_3a.snk_data,
			   &fixture->sink_5v_3a.common_data,
			   &fixture->sink_5v_3a.ops, fixture->tcpci_snk_emul),
		   NULL);

	/* Wait for PD negotiation and current ramp.
	 * TODO(b/213906889): Check message timing and contents.
	 */
	k_sleep(K_SECONDS(10));
}

static void
disconnect_sink_from_port(struct usb_pd_ctrl_msg_test_fixture *fixture)
{
	zassume_ok(tcpci_emul_disconnect_partner(fixture->tcpci_snk_emul),
		   NULL);
	k_sleep(K_SECONDS(1));
}

static void connect_src_to_port(struct usb_pd_ctrl_msg_test_fixture *fixture,
				int pdo_index)
{
	set_ac_enabled(true);
	zassume_ok(tcpci_drp_emul_connect_to_tcpci(
			   &fixture->source_5v_3a.data,
			   &fixture->source_5v_3a.src_data,
			   &fixture->source_5v_3a.snk_data,
			   &fixture->source_5v_3a.common_data,
			   &fixture->source_5v_3a.ops, fixture->tcpci_src_emul),
		   NULL);

	isl923x_emul_set_adc_vbus(
		fixture->charger_emul,
		PDO_FIXED_GET_VOLT(
			fixture->source_5v_3a.src_data.pdo[pdo_index]));

	k_sleep(K_SECONDS(10));
}

static void
disconnect_src_from_port(struct usb_pd_ctrl_msg_test_fixture *fixture)
{
	set_ac_enabled(false);
	zassume_ok(tcpci_emul_disconnect_partner(fixture->tcpci_src_emul),
		   NULL);
	isl923x_emul_set_adc_vbus(fixture->charger_emul, 0);
	k_sleep(K_SECONDS(1));
}

static void *usb_pd_ctrl_msg_setup(void)
{
	static struct usb_pd_ctrl_msg_test_fixture fixture;

	/* Get references for the emulators */
	fixture.tcpci_snk_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_ps8xxx_emul)));
	fixture.tcpci_src_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_emul)));
	fixture.charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	tcpci_emul_set_rev(fixture.tcpci_snk_emul, TCPCI_EMUL_REV2_0_VER1_1);

	/* Turn TCPCI rev 2 ON */
	tcpc_config[SNK_PORT].flags |= TCPC_FLAGS_TCPCI_REV2_0;
	tcpc_config[SRC_PORT].flags |= TCPC_FLAGS_TCPCI_REV2_0;

	return &fixture;
}

static void usb_pd_ctrl_msg_before(void *data)
{
	struct usb_pd_ctrl_msg_test_fixture *fixture = data;

	set_test_runner_tid();

	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	/* Initialized the sink to request 5V and 3A */
	tcpci_drp_emul_init(&fixture->sink_5v_3a);
	connect_sink_to_port(fixture);

	k_sleep(K_SECONDS(10));

	/* Initialize source */
	tcpci_drp_emul_init(&fixture->source_5v_3a);
	connect_src_to_port(fixture, 1);

	k_sleep(K_SECONDS(10));
}

static void usb_pd_ctrl_msg_after(void *data)
{
	struct usb_pd_ctrl_msg_test_fixture *fixture = data;

	disconnect_src_from_port(fixture);
	disconnect_sink_from_port(fixture);
}

ZTEST_SUITE(usb_pd_ctrl_msg_test, drivers_predicate_post_main,
	    usb_pd_ctrl_msg_setup, usb_pd_ctrl_msg_before,
	    usb_pd_ctrl_msg_after, NULL);

ZTEST_F(usb_pd_ctrl_msg_test, verify_pr_swap)
{
	struct usb_pd_ctrl_msg_test_fixture *fixture = this;

	struct ec_response_typec_status snk_resp = { 0 };
	struct ec_response_typec_status src_resp = { 0 };
	int rv = 0;

	snk_resp = host_cmd_typec_status(SNK_PORT);
	zassert_equal(PD_ROLE_SINK, snk_resp.power_role,
		      "SNK Returned power_role=%u", snk_resp.power_role);
	src_resp = host_cmd_typec_status(SRC_PORT);
	zassert_equal(PD_ROLE_SOURCE, src_resp.power_role,
		      "SRC Returned power_role=%u", src_resp.power_role);

	printf("Sending PR_SWAP request");

	/* Send PR_SWAP request */
	rv = tcpci_partner_send_control_msg(&fixture->source_5v_3a.common_data,
					    PD_CTRL_PR_SWAP, 0);
	zassert_ok(rv, "Failed to send PR_SWAP request, rv=%d", rv);

	k_sleep(K_SECONDS(10));

	snk_resp = host_cmd_typec_status(SNK_PORT);
	zassert_equal(PD_ROLE_SOURCE, snk_resp.power_role,
		      "SNK Returned power_role=%u", snk_resp.power_role);
	src_resp = host_cmd_typec_status(SRC_PORT);
	zassert_equal(PD_ROLE_SINK, src_resp.power_role,
		      "SRC Returned power_role=%u", src_resp.power_role);
}

ZTEST_F(usb_pd_ctrl_msg_test, verify_vconn_swap)
{
	struct usb_pd_ctrl_msg_test_fixture *fixture = this;

	struct ec_response_typec_status snk_resp = { 0 };
	struct ec_response_typec_status src_resp = { 0 };
	int rv = 0;

	snk_resp = host_cmd_typec_status(SNK_PORT);
	zassert_equal(PD_ROLE_VCONN_OFF, snk_resp.vconn_role,
		      "SNK Returned vconn_role=%u", snk_resp.vconn_role);
	src_resp = host_cmd_typec_status(SRC_PORT);
	zassert_equal(PD_ROLE_VCONN_SRC, src_resp.vconn_role,
		      "SRC Returned vconn_role=%u", src_resp.vconn_role);

	printf("Sending VCONN_SWAP request");

	/* Send VCONN_SWAP request */
	rv = tcpci_partner_send_control_msg(&fixture->source_5v_3a.common_data,
					    PD_CTRL_VCONN_SWAP, 0);
	zassert_ok(rv, "Failed to send VCONN_SWAP request, rv=%d", rv);

	k_sleep(K_SECONDS(10));

	snk_resp = host_cmd_typec_status(SNK_PORT);
	zassert_equal(PD_ROLE_VCONN_SRC, snk_resp.vconn_role,
		      "SNK Returned vconn_role=%u", snk_resp.vconn_role);
	src_resp = host_cmd_typec_status(SRC_PORT);
	zassert_equal(PD_ROLE_VCONN_OFF, src_resp.vconn_role,
		      "SRC Returned vconn_role=%u", src_resp.vconn_role);
}
