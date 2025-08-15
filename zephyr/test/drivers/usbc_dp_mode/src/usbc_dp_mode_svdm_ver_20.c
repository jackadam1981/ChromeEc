/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "ec_commands.h"
#include "ec_tasks.h"
#include "emul/emul_isl923x.h"
#include "emul/tcpc/emul_ps8xxx.h"
#include "emul/tcpc/emul_tcpci.h"
#include "host_command.h"
#include "tcpm/tcpci.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd_vdo.h"
#include "usb_prl_sm.h"
#include "usbc_dp_mode.h"

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

struct usbc_dp_mode_svdm_ver_20_fixture {
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
	struct tcpci_partner_data partner;
	struct tcpci_snk_emul_data snk_ext;
};

static void setup_passive_cable_svdm_ver_20(struct tcpci_partner_data *partner)
{
	/* Set up the partner as DP-capable with a passive cable */
	add_dp_discovery(partner, SVDM_VER_2_0);
	partner->cable = &passive_usb3_32;
	add_displayport_mode_responses(partner, SVDM_VER_2_0);
}

static void *usbc_dp_mode_setup_svdm_ver_20(void)
{
	passive_usb3_32.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS(SVDM_VER_2_0);

	static struct usbc_dp_mode_svdm_ver_20_fixture fixture;
	struct tcpci_partner_data *partner = &fixture.partner;
	struct tcpci_snk_emul_data *snk_ext = &fixture.snk_ext;

	tcpci_partner_init(partner, PD_REV30);
	partner->extensions = tcpci_snk_emul_init(snk_ext, partner, NULL);

	/* Get references for the emulators */
	fixture.tcpci_emul = EMUL_DT_GET(DT_NODELABEL(tcpci_emul));
	fixture.charger_emul = EMUL_DT_GET(DT_NODELABEL(isl923x_emul));

	return &fixture;
}

static void usbc_dp_mode_before(void *data)
{
	/* Set chipset on so the "AP" is on to give us commands */
	test_set_chipset_to_s0();
}

static void usbc_dp_mode_after_svdm_ver_20(void *data)
{
	struct usbc_dp_mode_svdm_ver_20_fixture *fix = data;

	/* return PD rev to 3.0 in case a test changed it */
	prl_set_rev(TEST_PORT, TCPCI_MSG_SOP_PRIME, PD_REV30);

	disconnect_sink_from_port(fix->tcpci_emul);
	tcpci_partner_common_enable_pd_logging(&fix->partner, false);
	tcpci_partner_common_clear_logged_msgs(&fix->partner);
}

ZTEST_SUITE(usbc_dp_mode_svdm_ver_20, drivers_predicate_post_main,
	    usbc_dp_mode_setup_svdm_ver_20, usbc_dp_mode_before,
	    usbc_dp_mode_after_svdm_ver_20, NULL);

ZTEST_F(usbc_dp_mode_svdm_ver_20, test_discovery_svdm_ver_20)
{
	setup_passive_cable_svdm_ver_20(&fixture->partner);
	/* But with DP mode response and modal operation set to true */
	fixture->partner.cable->identity_vdm[VDO_INDEX_IDH] |=
		VDO_MODAL_OPERATION_BIT;
	fixture->partner.cable->svids_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_SVID) |
		VDO_SVDM_VERS(SVDM_VER_2_0);
	fixture->partner.cable->svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	fixture->partner.cable->svids_vdos = VDO_INDEX_HDR + 2;
	fixture->partner.cable->modes_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_MODES) |
		VDO_SVDM_VERS(SVDM_VER_2_0);
	fixture->partner.cable->modes_vdm[VDO_INDEX_HDR + 1] =
		VDO_MODE_DP(MODE_DP_PIN_C | MODE_DP_PIN_D, 0, 1,
			    CABLE_RECEPTACLE, MODE_DP_GEN2, MODE_DP_SNK) |
		DPAM_VER_VDO(0x0);
	fixture->partner.cable->modes_vdos = VDO_INDEX_HDR + 2;

	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	uint8_t response_buffer[EC_LPC_HOST_PACKET_SIZE];
	struct ec_response_typec_discovery *discovery =
		(struct ec_response_typec_discovery *)response_buffer;

	/* Verify SOP discovery */
	host_cmd_typec_discovery(TEST_PORT, TYPEC_PARTNER_SOP, response_buffer,
				 sizeof(response_buffer));

	/* The host command does not count the VDM header in identity_count. */
	zassert_equal(discovery->identity_count,
		      fixture->partner.identity_vdos - 1,
		      "Expected %d identity VDOs, got %d",
		      fixture->partner.identity_vdos - 1,
		      discovery->identity_count);
	zassert_mem_equal(
		discovery->discovery_vdo, fixture->partner.identity_vdm + 1,
		discovery->identity_count * sizeof(*discovery->discovery_vdo),
		"Discovered SOP identity ACK did not match");
	zassert_equal(discovery->svid_count, 1, "Expected 1 SVID, got %d",
		      discovery->svid_count);
	zassert_equal(discovery->svids[0].svid, USB_SID_DISPLAYPORT,
		      "Expected SVID 0x%04x, got 0x%04x", USB_SID_DISPLAYPORT,
		      discovery->svids[0].svid);
	zassert_equal(discovery->svids[0].mode_count, 1,
		      "Expected 1 DP mode, got %d",
		      discovery->svids[0].mode_count);
	zassert_equal(discovery->svids[0].mode_vdo[0],
		      fixture->partner.modes_vdm[1],
		      "DP mode VDOs did not match");

	/* Verify SOP' discovery */
	host_cmd_typec_discovery(TEST_PORT, TYPEC_PARTNER_SOP_PRIME,
				 response_buffer, sizeof(response_buffer));

	/* The host command does not count the VDM header in identity_count. */
	zassert_equal(discovery->identity_count,
		      fixture->partner.cable->identity_vdos - 1,
		      "Expected %d identity VDOs, got %d",
		      fixture->partner.cable->identity_vdos - 1,
		      discovery->identity_count);
	zassert_mem_equal(discovery->discovery_vdo,
			  fixture->partner.cable->identity_vdm + 1,
			  discovery->identity_count *
				  sizeof(*discovery->discovery_vdo),
			  "Discovered SOP identity ACK did not match");
	zassert_equal(discovery->svid_count, 1, "Expected 1 SVID, got %d",
		      discovery->svid_count);
	zassert_equal(discovery->svids[0].svid, USB_SID_DISPLAYPORT,
		      "Expected SVID 0x%04x, got 0x%04x", USB_SID_DISPLAYPORT,
		      discovery->svids[0].svid);
	zassert_equal(discovery->svids[0].mode_count, 1,
		      "Expected 1 DP mode, got %d",
		      discovery->svids[0].mode_count);
	zassert_equal(discovery->svids[0].mode_vdo[0],
		      fixture->partner.cable->modes_vdm[1],
		      "DP mode VDOs did not match");

	/* Verify established SVDM version */
	zassert_equal(pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP), SVDM_VER_2_0,
		      "Expected SVDM version 2.0 for SOP, got %d",
		      pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP));
	zassert_equal(pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP_PRIME),
		      SVDM_VER_2_0,
		      "Expected SVDM version 2.0 for SOP', got %d",
		      pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP_PRIME));
}

ZTEST_F(usbc_dp_mode_svdm_ver_20, test_dp21_entry_passive_32)
{
	setup_passive_cable_svdm_ver_20(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	struct ec_response_typec_status status;

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Unexpected starting mux: 0x%02x",
		      status.mux_state);

	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we entered DP mode */
	/* TODO: b/418824261 - Assert on the message sequence. */
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to see DP set");

	zassert_equal(status.sop_revision, PD_SPEC_REVISION,
		      "Wrong PD spec SOP revision received");

	zassert_equal(status.sop_prime_revision, PD_SPEC_REVISION,
		      "Wrong PD spec SOPP revision received");

	/* Verify established SVDM version */
	zassert_equal(pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP), SVDM_VER_2_0,
		      "Expected SVDM version 2.0 for SOP, got %d",
		      pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP));
	zassert_equal(pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP_PRIME),
		      SVDM_VER_2_0,
		      "Expected SVDM version 2.0 for SOP', got %d",
		      pd_get_vdo_ver(TEST_PORT, TCPCI_MSG_SOP_PRIME));
}
