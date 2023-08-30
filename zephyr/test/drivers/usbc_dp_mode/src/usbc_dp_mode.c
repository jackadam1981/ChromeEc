/* Copyright 2023 The ChromiumOS Authors
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
#include "usb_prl_sm.h"

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

/* Non passive or Active cable */
static struct tcpci_cable_data non_active_non_passive = {
	.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR,
	.identity_vdm[VDO_INDEX_IDH] = VDO_IDH(
		/* USB host */ false, /* USB device */ false, IDH_PTYPE_AMA,
		/* modal operation */ false, USB_VID_GOOGLE),
	.identity_vdm[VDO_INDEX_CSTAT] = 0,
	.identity_vdm[VDO_INDEX_PRODUCT] = VDO_PRODUCT(0x1234, 0xABCD),
	.identity_vdm[VDO_INDEX_PTYPE_CABLE1] =
		VDO_REV30_PASSIVE(USB_R30_SS_U32_U40_GEN2, USB_VBUS_CUR_3A,
				  USB_REV30_LATENCY_1m, USB_REV30_TYPE_C),
	.identity_vdos = VDO_INDEX_PTYPE_CABLE1 + 1,

};

/* Passive cable with USB3 gen 2 speed */
static struct tcpci_cable_data passive_usb3_32 = {
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

/* Passive cable with USB4 speed */
static struct tcpci_cable_data passive_usb3_4 = {
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

/* Passive cable with USB4 speed and modal operation */
static struct tcpci_cable_data passive_usb3_4_modal = {
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

/* Active cable base functions should add VDO 1 and VDO 2*/
static struct tcpci_cable_data active_optical_redriver = {
	.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR,
	.identity_vdm[VDO_INDEX_IDH] = VDO_IDH(
		/* USB host */ false, /* USB device */ false, IDH_PTYPE_ACABLE,
		/* modal operation */ true, USB_VID_GOOGLE),
	.identity_vdm[VDO_INDEX_CSTAT] = 0,
	.identity_vdm[VDO_INDEX_PRODUCT] = VDO_PRODUCT(0x1234, 0xABCD),
	/* Set to CABLE_2 with the assumption the caller will add these values
	 */
	.identity_vdos = VDO_INDEX_PTYPE_CABLE2 + 1,

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

static void
setup_active_optical_redriver_cable_test(struct tcpci_partner_data *partner)
{
	union active_cable_vdo1_rev30 optical_redriver = {
		.ss = USB_R30_SS_U40_GEN3,
		.sop_p_p = 0, /* SOP'' Not Present */
		.vbus_cable = BIT(0), /* VBUS allowed */
		.vbus_cur = USB_VBUS_CUR_3A,
		.sbu_type = 0, /* passive SBU */
		.sbu_support = BIT(0), /* SBU not supported */
		.vbus_max = 0, /* 20V */
		.termination = (BIT(0) | BIT(1)), /* Both ends active */
		.latency = USB_REV30_LATENCY_1m,
		.connector = USB_REV30_TYPE_C,
	};

	union active_cable_vdo2_rev30 optical_redriver_vdo2 = {
		.usb_gen = BIT(0), /* Gen 2 or Higher */
		.a_cable_type = BIT(0), /* Optically iso active cable */
		.usb_lanes = BIT(0), /* Two lanes */
		.usb_32_support = 0, /* USB 3.2 supported */
		.usb_20_support = USB2_NOT_SUPPORTED,
		.usb_20_hub_hop = 0, /* Don't Care */
		.usb_40_support = USB4_SUPPORTED,
		.active_elem = ACTIVE_REDRIVER,
		.physical_conn = BIT(0), /* Optical Conn */
		.u3_to_u0 = 0, /* Direct Conn */
		.u3_power = 0, /* >10mW */
		.shutdown_temp = 0xff, /* Max temp cause we don't care */
		.max_operating_temp = 0xff, /* Max temp cause we don't care */
	};

	active_optical_redriver.identity_vdm[VDO_INDEX_PTYPE_CABLE1] =
		optical_redriver.raw_value;
	active_optical_redriver.identity_vdm[VDO_INDEX_PTYPE_CABLE2] =
		optical_redriver_vdo2.raw_value;

	add_dp_21_discovery(partner);
	partner->cable = &active_optical_redriver;
	add_displayport_mode_responses(partner);
}

static void setup_non_active_non_passive(struct tcpci_partner_data *partner)
{
	/* Set up the partner as DP-capable with a passive cable */
	add_dp_21_discovery(partner);
	partner->cable = &non_active_non_passive;
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
    /* But with DP mode response */
	fixture->partner.cable->svids_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_SVID) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	fixture->partner.cable->svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	fixture->partner.cable->svids_vdos = VDO_INDEX_HDR + 2;
	fixture->partner.cable->modes_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_MODES) |
		VDO_SVDM_VERS_MAJOR(SVDM_VER_2_1) | VDM_VERS_MINOR;
	fixture->partner.cable->modes_vdm[VDO_INDEX_HDR + 1] =
		VDO_MODE_DP(MODE_DP_PIN_C | MODE_DP_PIN_D, 0, 1,
			    CABLE_RECEPTACLE, MODE_DP_GEN2, MODE_DP_SNK) |
		DPAM_VER_VDO(0x1);
	fixture->partner.cable->modes_vdos = VDO_INDEX_HDR + 2;

	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	uint8_t response_buffer[EC_LPC_HOST_PACKET_SIZE];
	struct ec_response_typec_discovery *discovery =
		(struct ec_response_typec_discovery *)response_buffer;

	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

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
	host_cmd_typec_discovery(TEST_PORT, TYPEC_PARTNER_SOP_PRIME, response_buffer,
				 sizeof(response_buffer));

	/* The host command does not count the VDM header in identity_count. */
	zassert_equal(discovery->identity_count,
		      fixture->partner.cable->identity_vdos - 1,
		      "Expected %d identity VDOs, got %d",
		      fixture->partner.cable->identity_vdos - 1,
		      discovery->identity_count);
	zassert_mem_equal(
		discovery->discovery_vdo, fixture->partner.cable->identity_vdm + 1,
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
		      fixture->partner.cable->modes_vdm[1],
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

ZTEST_F(usbc_dp_mode, test_dp21_entry_tbt_optical_redriver)
{
	struct ec_response_typec_status status;

	setup_active_optical_redriver_cable_test(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Unexpected starting mux: 0x%02x",
		      status.mux_state);

	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we did not enter DP mode */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_not_equal((status.mux_state & USB_MUX_CHECK_MASK),
			  USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
			  "DP mode entered incorrectly");

	/* Exit mode and prepare for next active test */
	host_cmd_typec_control_exit_modes(TEST_PORT);
}

ZTEST_F(usbc_dp_mode, test_dp21_entry_active_redriver)
{
	union tbt_mode_resp_cable cable_resp;
	struct ec_response_typec_status status;
	struct pd_discovery *disc;

	setup_active_optical_redriver_cable_test(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	disc = pd_get_am_discovery_and_notify_access(TEST_PORT,
						     TCPCI_MSG_SOP_PRIME);

	disc->identity.idh.product_type = IDH_PTYPE_ACABLE;
	disc->identity.product_t2.a2_rev30.active_elem = ACTIVE_REDRIVER;
	disc->identity.product_t1.p_rev30.ss = USB_R30_SS_U32_U40_GEN2;
	prl_set_rev(TEST_PORT, TCPCI_MSG_SOP_PRIME, PD_REV30);

	/* Set cable VDO */
	disc->svid_cnt = 1;
	disc->svids[0].svid = USB_VID_INTEL;
	disc->svids[0].discovery = PD_DISC_COMPLETE;
	disc->svids[0].mode_cnt = 1;
	cable_resp.tbt_alt_mode = TBT_ALTERNATE_MODE;
	cable_resp.tbt_cable_speed = TBT_SS_RES_0;
	cable_resp.tbt_rounded = TBT_GEN3_NON_ROUNDED;
	cable_resp.tbt_cable = TBT_CABLE_NON_OPTICAL;
	cable_resp.retimer_type = USB_NOT_RETIMER;
	cable_resp.lsrx_comm = BIDIR_LSRX_COMM;
	cable_resp.tbt_active_passive = TBT_CABLE_PASSIVE;
	disc->svids[0].mode_vdo[0] = cable_resp.raw_value;

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we sent a single DP SOP EnterMode. */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to see DP set with tbt active redriver");

	/* Exit DP Mode and Verify it Exited */
	host_cmd_typec_control_exit_modes(TEST_PORT);
	k_sleep(K_MSEC(1000));

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Failed to return to USB mode");
}

ZTEST_F(usbc_dp_mode, test_dp21_entry_active_retimer)
{
	union tbt_mode_resp_cable cable_resp;
	struct ec_response_typec_status status;
	struct pd_discovery *disc;

	setup_active_optical_redriver_cable_test(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	disc = pd_get_am_discovery_and_notify_access(TEST_PORT,
						     TCPCI_MSG_SOP_PRIME);

	/* Set cable VDO to retimer enabled */
	cable_resp.raw_value = disc->svids[0].mode_vdo[0];
	cable_resp.retimer_type = USB_RETIMER;
	disc->svids[0].mode_vdo[0] = cable_resp.raw_value;

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Should not enter DP mode with retimer_type as retimer */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_not_equal((status.mux_state & USB_MUX_CHECK_MASK),
			  USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
			  "Entered DP mode with tbt retimer incorrectly");

	disc->identity.idh.modal_support = 0; /* disable modal support */
	cable_resp.raw_value = disc->svids[0].mode_vdo[0];
	cable_resp.retimer_type = USB_NOT_RETIMER; /* Return to not retimer */
	disc->svids[0].mode_vdo[0] = cable_resp.raw_value;

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Should not enter DP mode with retimer_type as retimer */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_not_equal((status.mux_state & USB_MUX_CHECK_MASK),
			  USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
			  "Entered DP mode with tbt retimer incorrectly");
}

ZTEST_F(usbc_dp_mode, test_dp21_dp_cable)
{
	union dp_mode_resp_cable cable_resp;
	struct ec_response_typec_status status;
	struct pd_discovery *disc;

	setup_active_optical_redriver_cable_test(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	disc = pd_get_am_discovery_and_notify_access(TEST_PORT,
						     TCPCI_MSG_SOP_PRIME);

	disc->identity.idh.product_type = IDH_PTYPE_ACABLE;
	disc->identity.product_t2.a2_rev30.active_elem = ACTIVE_REDRIVER;
	disc->identity.product_t1.p_rev30.ss = USB_R30_SS_U32_U40_GEN2;
	prl_set_rev(TEST_PORT, TCPCI_MSG_SOP_PRIME, PD_REV30);

	/* Set cable VDO */
	disc->svid_cnt = 1;
	disc->svids[0].svid = USB_SID_DISPLAYPORT;
	disc->svids[0].discovery = PD_DISC_COMPLETE;
	disc->svids[0].mode_cnt = 1;
	cable_resp.uhbr13_5_support = 0;
	cable_resp.active_comp = DP21_OPTICAL_CABLE;
	cable_resp.dpam_ver = DPAM_VERSION_21;
	disc->svids[0].mode_vdo[0] = cable_resp.raw_value;

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Verify we sent a single DP SOP EnterMode. */
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to see DP set with tbt active redriver");
}

ZTEST_F(usbc_dp_mode, test_dp21_non_emark)
{
	struct ec_response_typec_status status;

	setup_non_active_non_passive(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Should enter DP mode without active or passive cable*/
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed to enter DP mode with non Emark cable");

	/* Exit DP Mode and Verify it Exited */
	host_cmd_typec_control_exit_modes(TEST_PORT);
	k_sleep(K_MSEC(1000));

	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED, "Failed to return to USB mode");
}

ZTEST_F(usbc_dp_mode, test_dp21_usb20)
{
	struct pd_discovery *disc;
	struct ec_response_typec_status status;

	setup_non_active_non_passive(&fixture->partner);
	connect_sink_to_port(&fixture->partner, fixture->tcpci_emul,
			     fixture->charger_emul);

	disc = pd_get_am_discovery_and_notify_access(TEST_PORT,
						     TCPCI_MSG_SOP_PRIME);

	/* change to passive cable , USB 2 only and rev20 instead of 30 */
	disc->identity.idh.product_type = IDH_PTYPE_PCABLE;
	disc->identity.product_t1.p_rev20.ss = USB_R20_SS_U2_ONLY;
	prl_set_rev(TEST_PORT, TCPCI_MSG_SOP_PRIME, PD_REV20);

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Should not enter DP mode with passive cable and usb2 only*/
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_not_equal((status.mux_state & USB_MUX_CHECK_MASK),
			  USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
			  "Entered DP mode with usb 2 only");

	/* DP mode is entered when USB2 but cable supports USB3 */
	disc->identity.product_t1.p_rev20.ss = USB_R20_SS_U31_GEN1;

	tcpci_partner_common_enable_pd_logging(&fixture->partner, true);
	host_cmd_typec_control_enter_mode(TEST_PORT, TYPEC_MODE_DP);
	k_sleep(K_MSEC(1000));

	/* Should not enter DP mode with passive cable and usb2 only*/
	tcpci_partner_common_enable_pd_logging(&fixture->partner, false);
	status = host_cmd_typec_status(TEST_PORT);
	zassert_equal((status.mux_state & USB_MUX_CHECK_MASK),
		      USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED,
		      "Failed DP mode with usb 2 with 3 support");
}
