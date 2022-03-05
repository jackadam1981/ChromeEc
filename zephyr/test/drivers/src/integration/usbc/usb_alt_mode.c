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

struct usbc_alt_mode_fixture {
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
	struct tcpci_snk_emul partner_emul;

	/* Partner VDM responses */
	int partner_identity_vdos;
	uint32_t partner_identity_vdm[VDO_MAX_SIZE];
	int partner_svids_vdos;
	uint32_t partner_svids_vdm[VDO_MAX_SIZE];
	int partner_modes_vdos;
	uint32_t partner_modes_vdm[VDO_MAX_SIZE];
};

static void connect_partner_to_port(struct usbc_alt_mode_fixture *fixture)
{
	const struct emul *tcpc_emul = fixture->tcpci_emul;
	struct tcpci_snk_emul *partner_emul = &fixture->partner_emul;

#if 0
	set_ac_enabled(true);
#endif
	isl923x_emul_set_adc_vbus(fixture->charger_emul, 0);
	tcpci_emul_set_reg(fixture->tcpci_emul, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_VBUS_DET);
	tcpci_emul_set_reg(fixture->tcpci_emul, TCPC_REG_EXT_STATUS,
			   TCPC_REG_EXT_STATUS_SAFE0V);
	tcpci_tcpc_alert(0);
	zassume_ok(tcpci_snk_emul_connect_to_tcpci(&partner_emul->data,
				&partner_emul->common_data, &partner_emul->ops,
				tcpc_emul),
		   NULL);

#if 0
	isl923x_emul_set_adc_vbus(
		fixture->charger_emul,
		PDO_FIXED_GET_VOLT(fixture->partner_emul.src_data.pdo[0]));
#endif

	/* Wait for PD negotiation and current ramp.
	 * TODO(b/213906889): Check message timing and contents.
	 */
	k_sleep(K_SECONDS(10));
}

static void disconnect_partner_from_port(struct usbc_alt_mode_fixture *fixture)
{
#if 0
	set_ac_enabled(false);
#endif
	zassume_ok(tcpci_emul_disconnect_partner(fixture->tcpci_emul), NULL);
	isl923x_emul_set_adc_vbus(fixture->charger_emul, 0);
	k_sleep(K_SECONDS(1));
}

static void *usbc_alt_mode_setup(void)
{
	static struct usbc_alt_mode_fixture fixture;

	tcpc_config[0].flags |= TCPC_FLAGS_TCPCI_REV2_0;

	/* Get references for the emulators */
	fixture.tcpci_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_emul)));
	tcpci_emul_set_rev(fixture.tcpci_emul, TCPCI_EMUL_REV2_0_VER1_1);
	fixture.charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	/* Set up discovery responses for DP adapter. */
	fixture.partner_identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT);
	fixture.partner_identity_vdm[VDO_INDEX_IDH] =
		VDO_IDH(/* USB host */ false, /* USB device */ false,
				IDH_PTYPE_AMA, /* modal operation */ true,
				USB_VID_GOOGLE);
	fixture.partner_identity_vdm[VDO_INDEX_CSTAT] = 0xabcdabcd;
	fixture.partner_identity_vdm[VDO_INDEX_PRODUCT] =
		VDO_PRODUCT(0x1234, 0x5678);
	/* Hardware version 1, firmware version 2 */
	fixture.partner_identity_vdm[VDO_INDEX_AMA] = 0x12000000;
	fixture.partner_identity_vdos = VDO_INDEX_AMA + 1;

	fixture.partner_svids_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_SVID);
	fixture.partner_svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	fixture.partner_svids_vdos = VDO_INDEX_HDR + 2;

	fixture.partner_modes_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_MODES);
	fixture.partner_modes_vdm[VDO_INDEX_HDR + 1] =
		/* Copied from Hoho */
		VDO_MODE_DP(0, MODE_DP_PIN_C, 1, CABLE_PLUG, MODE_DP_V13,
				MODE_DP_SNK);
	fixture.partner_modes_vdos = VDO_INDEX_HDR + 2;

	/* Initialized the charger to supply 20V and 3A */
	tcpci_snk_emul_init(&fixture.partner_emul);
	fixture.partner_emul.data.pdo[1] =
		PDO_FIXED(5000, 3000, PDO_FIXED_UNCONSTRAINED);

	return &fixture;
}

static void usbc_alt_mode_before(void *data)
{
	/* Set chipset to ON, this will set TCPM to DRP */
	test_set_chipset_to_s0();

	/* TODO(b/214401892): Check why need to give time TCPM to spin */
	k_sleep(K_SECONDS(1));

	connect_partner_to_port((struct usbc_alt_mode_fixture *)data);
}

static void usbc_alt_mode_after(void *data)
{
	disconnect_partner_from_port((struct usbc_alt_mode_fixture *)data);
}

#if 0
static void *integration_usb_alt_mode_setup(void)
{
	static struct usbc_alt_mode_fixture fixture;

	fixture.tcpci_generic_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_emul)));
	fixture.charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	fixture.partner_identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT);
	fixture.partner_identity_vdm[VDO_INDEX_IDH] =
		VDO_IDH(/* USB host */ false, /* USB device */ false,
				IDH_PTYPE_AMA, /* modal operation */ true,
				USB_VID_GOOGLE);
	fixture.partner_identity_vdm[VDO_INDEX_CSTAT] = 0xabcdabcd;
	fixture.partner_identity_vdm[VDO_INDEX_PRODUCT] =
		VDO_PRODUCT(0x1234, 0x5678);
	/* Hardware version 1, firmware version 2 */
	fixture.partner_identity_vdm[VDO_INDEX_AMA] = 0x12000000;
	fixture.partner_identity_vdos = VDO_INDEX_AMA + 1;

	fixture.partner_svids_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_SVID);
	fixture.partner_svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	fixture.partner_svids_vdos = VDO_INDEX_HDR + 2;

	fixture.partner_modes_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_MODES);
	fixture.partner_modes_vdm[VDO_INDEX_HDR + 1] =
		/* Copied from Hoho */
		VDO_MODE_DP(0, MODE_DP_PIN_C, 1, CABLE_PLUG, MODE_DP_V13,
				MODE_DP_SNK);
	fixture.partner_modes_vdos = VDO_INDEX_HDR + 2;

	return &fixture;
}

static void integration_usb_alt_mode_before(void *state)
{
	struct usbc_alt_mode_fixture *fixture = state;
	const struct emul *tcpc_emul = fixture->tcpci_generic_emul;
	struct tcpci_drp_emul *partner_emul = &fixture->partner_emul;

	zassume_ok(tcpc_config[USBC_PORT_C0].drv->init(USBC_PORT_C0), NULL);
	tcpci_emul_set_rev(tcpc_emul, TCPCI_EMUL_REV1_0_VER1_0);
	pd_set_suspend(USBC_PORT_C0, false);
	/* Reset to disconnected state. */
	zassume_ok(tcpci_emul_disconnect_partner(tcpc_emul), NULL);

	/* Attach emulated partner. */
	tcpci_drp_emul_init(partner_emul);
	tcpci_partner_set_discovery_info(
			&partner_emul->common_data,
			fixture->partner_identity_vdos,
			fixture->partner_identity_vdm,
			fixture->partner_svids_vdos,
			fixture->partner_svids_vdm,
			fixture->partner_modes_vdos,
			fixture->partner_modes_vdm);
	set_ac_enabled(true);
	zassume_ok(tcpci_drp_emul_connect_to_tcpci(&partner_emul->data,
			   &partner_emul->src_data, &partner_emul->snk_data,
			   &partner_emul->common_data, &partner_emul->ops,
			   tcpc_emul),
		   NULL);
	isl923x_emul_set_adc_vbus(fixture->charger_emul, 5000);

	/* TODO(b/219562077): Drive DP mode entry. */

	/* Wait for current ramp. */
	k_sleep(K_SECONDS(10));

#if 0
	/* Set chipset to ON, this will set TCPM to allow mode entry. */
	test_set_chipset_to_s0();
#endif
}

static void integration_usb_alt_mode_after(void *state)
{
	struct usbc_alt_mode_fixture *fixture = state;

	const struct emul *tcpc_emul = fixture->tcpci_generic_emul;
	const struct emul *charger_emul = fixture->charger_emul;

	set_ac_enabled(false);
	tcpci_emul_disconnect_partner(tcpc_emul);

	/* Give time to actually disconnect */
	k_sleep(K_SECONDS(1));

	/* Reset vbus to 0mV */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(charger_emul, 0);
}
#endif

ZTEST_F(usbc_alt_mode, verify_discovery)
{
	struct ec_response_typec_discovery discovery =
		host_cmd_typec_discovery(USBC_PORT_C0, TYPEC_PARTNER_SOP);

	zassert_equal(discovery.identity_count, this->partner_identity_vdos,
			"Expected %d identity VDOs, got %d",
			this->partner_identity_vdos, discovery.identity_count);
}

ZTEST_SUITE(usbc_alt_mode, drivers_predicate_post_main, usbc_alt_mode_setup,
		usbc_alt_mode_before, usbc_alt_mode_after, NULL);
