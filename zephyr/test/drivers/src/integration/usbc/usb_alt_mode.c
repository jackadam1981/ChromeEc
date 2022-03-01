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
	const struct emul *tcpci_generic_emul;
	const struct emul *charger_emul;
	struct tcpci_drp_emul partner_emul;

	/* Partner VDM responses */
	int partner_identity_vdos;
	uint32_t partner_identity_vdm[VDO_MAX_SIZE];
	int partner_svids_vdos;
	uint32_t partner_svids_vdm[VDO_MAX_SIZE];
	int partner_modes_vdos;
	uint32_t partner_modes_vdm[VDO_MAX_SIZE];
};

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
	/* TODO: Set up discovery info */
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

	tcpci_emul_disconnect_partner(tcpc_emul);

	/* Give time to actually disconnect */
	k_sleep(K_SECONDS(1));

	/* Reset vbus to 0mV */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(charger_emul, 0);
}

ZTEST_F(usbc_alt_mode, verify_discovery)
{
	struct ec_response_typec_discovery discovery =
		host_cmd_typec_discovery(USBC_PORT_C0, TYPEC_PARTNER_SOP);

	zassert_equal(discovery.identity_count, this->partner_identity_vdos,
			"Expected %d identity VDOs, got %d",
			discovery.identity_count, this->partner_identity_vdos);
}

ZTEST_SUITE(usbc_alt_mode, NULL,
	    integration_usb_alt_mode_setup,
	    integration_usb_alt_mode_before,
	    integration_usb_alt_mode_after, NULL);
