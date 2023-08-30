/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "ec_tasks.h"
#include "emul/emul_isl923x.h"
#include "emul/tcpc/emul_ps8xxx.h"
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "host_command.h"
#include "tcpm/tcpci.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd_vdo.h"

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>
#define TEST_PORT USBC_PORT_C0

/* Remove polarity for any mux checks */
#define USB_MUX_CHECK_MASK ~USB_PD_MUX_POLARITY_INVERTED
#define DPAM_VER_VDO(x) (x << 30)

struct usbc_dp_mode_fixture {
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
	struct tcpci_partner_data partner;
	struct tcpci_snk_emul_data snk_ext;
};

struct tcpci_cable_data passive_usb3_32 = {
	.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR,
	.identity_vdm[VDO_INDEX_IDH] = VDO_IDH(
		/* USB host */ false, /* USB device */ false, IDH_PTYPE_PCABLE,
		/* modal operation */ false, USB_VID_GOOGLE),
	.identity_vdm[VDO_INDEX_CSTAT] = 0,
	.identity_vdm[VDO_INDEX_PRODUCT] = VDO_PRODUCT(0x1234, 0xABCD),
	.identity_vdm[VDO_INDEX_PTYPE_CABLE1] =
		VDO_REV30_PASSIVE(USB_R30_SS_U32_U40_GEN2, USB_VBUS_CUR_3A,
				  USB_REV30_LATENCY_1m, USB_REV30_TYPE_C),
	.identity_vdos = VDO_INDEX_PTYPE_CABLE1 + 1,

};

struct tcpci_cable_data passive_usb3_4 = {
	.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR,
	.identity_vdm[VDO_INDEX_IDH] = VDO_IDH(
		/* USB host */ false, /* USB device */ false, IDH_PTYPE_PCABLE,
		/* modal operation */ false, USB_VID_GOOGLE),
	.identity_vdm[VDO_INDEX_CSTAT] = 0,
	.identity_vdm[VDO_INDEX_PRODUCT] = VDO_PRODUCT(0x1234, 0xABCD),
	.identity_vdm[VDO_INDEX_PTYPE_CABLE1] =
		VDO_REV30_PASSIVE(USB_R30_SS_U40_GEN3, USB_VBUS_CUR_3A,
				  USB_REV30_LATENCY_1m, USB_REV30_TYPE_C),
	.identity_vdos = VDO_INDEX_PTYPE_CABLE1 + 1,

};

struct tcpci_cable_data passive_usb3_4_modal = {
	.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR,
	.identity_vdm[VDO_INDEX_IDH] = VDO_IDH(
		/* USB host */ false, /* USB device */ false, IDH_PTYPE_PCABLE,
		/* modal operation */ true, USB_VID_GOOGLE),
	.identity_vdm[VDO_INDEX_CSTAT] = 0,
	.identity_vdm[VDO_INDEX_PRODUCT] = VDO_PRODUCT(0x1234, 0xABCD),
	.identity_vdm[VDO_INDEX_PTYPE_CABLE1] =
		VDO_REV30_PASSIVE(USB_R30_SS_U40_GEN3, USB_VBUS_CUR_3A,
				  USB_REV30_LATENCY_1m, USB_REV30_TYPE_C),
	.identity_vdos = VDO_INDEX_PTYPE_CABLE1 + 1,

};

static void add_dp_21_discovery(struct tcpci_partner_data *partner)
{
	/* Add Discover Identity response */
	partner->identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	partner->identity_vdm[VDO_INDEX_IDH] = VDO_IDH(
		/* USB host */ false, /* USB device */ true, IDH_PTYPE_HUB,
		/* modal operation */ true, USB_VID_GOOGLE);
	partner->identity_vdm[VDO_INDEX_CSTAT] = 0;
	partner->identity_vdm[VDO_INDEX_PRODUCT] = VDO_PRODUCT(0x1234, 0x5678);
	/* Hardware version 1, firmware version 2 */
	partner->identity_vdm[VDO_INDEX_PTYPE_UFP1_VDO] = VDO_UFP1(
		(VDO_UFP1_CAPABILITY_USB20 | VDO_UFP1_CAPABILITY_USB32),
		USB_TYPEC_RECEPTACLE, VDO_UFP1_ALT_MODE_RECONFIGURE,
		USB_R30_SS_U32_U40_GEN2);
	partner->identity_vdos = VDO_INDEX_PTYPE_UFP1_VDO + 1;

	/* Add Discover Modes response */
	/* Support one mode for DisplayPort VID.*/
	partner->modes_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_MODES) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	partner->modes_vdm[VDO_INDEX_HDR + 1] =
		VDO_MODE_DP(MODE_DP_PIN_C | MODE_DP_PIN_D, 0, 1,
			    CABLE_RECEPTACLE, MODE_DP_GEN2, MODE_DP_SNK) |
		DPAM_VER_VDO(0x1);
	partner->modes_vdos = VDO_INDEX_HDR + 2;

	/* Add Discover SVIDs response */
	/* Support DisplayPort VID. */
	partner->svids_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_SVID) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	partner->svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	partner->svids_vdos = VDO_INDEX_HDR + 2;
}

static void add_displayport_mode_responses(struct tcpci_partner_data *partner)
{
	/* Add DisplayPort EnterMode response */
	partner->enter_mode_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_ENTER_MODE) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	partner->enter_mode_vdos = VDO_INDEX_HDR + 1;

	/* Add DisplayPort StatusUpdate response */
	partner->dp_status_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DP_STATUS) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	partner->dp_status_vdm[VDO_INDEX_HDR + 1] =
		VDO_DP_STATUS(0, /* IRQ_HPD */
			      false, /* HPD_HI|LOW - Changed*/
			      0, /* request exit DP */
			      0, /* request exit USB */
			      1, /* MF pref */
			      true, /* DP Enabled */
			      0, /* power low e.g. normal */
			      0x2 /* Connected as Sink */);
	partner->dp_status_vdos = VDO_INDEX_HDR + 2;

	/* Add DisplayPort Configure Response */
	partner->dp_config_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DP_CONFIG) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	partner->dp_config_vdos = VDO_INDEX_HDR + 1;
}

static void setup_passive_cable(struct tcpci_partner_data *partner)
{
	/* Set up the partner as DP-capable with a passive cable */
	add_dp_21_discovery(partner);
	partner->cable = &passive_usb3_32;
	add_displayport_mode_responses(partner);
}

static void setup_passive_cable_u40(struct tcpci_partner_data *partner)
{
	/* Set up the partner as DP-capable with a passive cable */
	add_dp_21_discovery(partner);
	partner->cable = &passive_usb3_4;
	add_displayport_mode_responses(partner);
}

static void setup_passive_cable_u40_modal(struct tcpci_partner_data *partner)
{
	/* Set up the partner as DP-capable with a passive cable */
	add_dp_21_discovery(partner);
	partner->cable = &passive_usb3_4_modal;
	add_displayport_mode_responses(partner);
}

static void *usbc_dp_mode_setup(void)
{
	static struct usbc_dp_mode_fixture fixture;
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

static void usbc_dp_mode_after(void *data)
{
	struct usbc_dp_mode_fixture *fix = data;

	disconnect_source_from_port(fix->tcpci_emul, fix->charger_emul);
	tcpci_partner_common_clear_logged_msgs(&fix->partner);
}

ZTEST_SUITE(usbc_dp_mode, drivers_predicate_post_main, usbc_dp_mode_setup,
	    usbc_dp_mode_before, usbc_dp_mode_after, NULL);

ZTEST_F(usbc_dp_mode, test_verify_discovery)
{
	setup_passive_cable(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	uint8_t response_buffer[EC_LPC_HOST_PACKET_SIZE];
	struct ec_response_typec_discovery *discovery =
		(struct ec_response_typec_discovery *)response_buffer;

	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

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
}

ZTEST_F(usbc_dp_mode, test_dp21_entry_passive_32)
{
	setup_passive_cable(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	struct ec_response_typec_status status;

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Unexpected starting mux: 0x%02x",
		      status.mux_state);

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we sent a single DP SOP EnterMode. */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to see DP set");
}

ZTEST_F(usbc_dp_mode, test_dp21_entry_passive_u40)
{
	setup_passive_cable_u40(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	struct ec_response_typec_status status;

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Unexpected starting mux: 0x%02x",
		      status.mux_state);

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we sent a single DP SOP EnterMode. */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to see DP set");
}

ZTEST_F(usbc_dp_mode, test_dp21_entry_passive_u40_modal)
{
	setup_passive_cable_u40_modal(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	struct ec_response_typec_status status;

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Unexpected starting mux: 0x%02x",
		      status.mux_state);

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we sent a single DP SOP EnterMode. */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to see DP set");
}
