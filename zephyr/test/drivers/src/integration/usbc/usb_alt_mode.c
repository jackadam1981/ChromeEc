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

	tcpci_snk_emul_init(&fixture.partner_emul);

	/* Get references for the emulators */
	fixture.tcpci_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(tcpci_emul)));
	tcpci_emul_set_rev(fixture.tcpci_emul, TCPCI_EMUL_REV2_0_VER1_1);
	fixture.charger_emul =
		emul_get_binding(DT_LABEL(DT_NODELABEL(isl923x_emul)));

	/* Set up discovery responses for DP adapter. */
	fixture.partner_emul.common_data.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT);
	fixture.partner_emul.common_data.identity_vdm[VDO_INDEX_IDH] =
		VDO_IDH(/* USB host */ false, /* USB device */ false,
				IDH_PTYPE_AMA, /* modal operation */ true,
				USB_VID_GOOGLE);
	fixture.partner_emul.common_data.identity_vdm[VDO_INDEX_CSTAT] = 0xabcdabcd;
	fixture.partner_emul.common_data.identity_vdm[VDO_INDEX_PRODUCT] =
		VDO_PRODUCT(0x1234, 0x5678);
	/* Hardware version 1, firmware version 2 */
	fixture.partner_emul.common_data.identity_vdm[VDO_INDEX_AMA] = 0x12000000;
	fixture.partner_emul.common_data.identity_vdos = VDO_INDEX_AMA + 1;

	fixture.partner_emul.common_data.svids_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_SVID);
	fixture.partner_emul.common_data.svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	fixture.partner_emul.common_data.svids_vdos = VDO_INDEX_HDR + 2;

	fixture.partner_emul.common_data.modes_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
				VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_MODES);
	fixture.partner_emul.common_data.modes_vdm[VDO_INDEX_HDR + 1] =
		/* Copied from Hoho */
		VDO_MODE_DP(0, MODE_DP_PIN_C, 1, CABLE_PLUG, MODE_DP_V13,
				MODE_DP_SNK);
	fixture.partner_emul.common_data.modes_vdos = VDO_INDEX_HDR + 2;

	/* Initialized the charger to supply 20V and 3A */
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

ZTEST_F(usbc_alt_mode, verify_discovery)
{
	uint8_t array[EC_LPC_HOST_PACKET_SIZE];
	struct ec_response_typec_discovery *discovery =
		(struct ec_response_typec_discovery *) array;
	host_cmd_typec_discovery(USBC_PORT_C0, TYPEC_PARTNER_SOP, discovery);

	/* The host command does not count the VDM header in identity_count. */
	zassert_equal(discovery->identity_count,
			this->partner_emul.common_data.identity_vdos - 1,
			"Expected %d identity VDOs, got %d",
			this->partner_emul.common_data.identity_vdos - 1,
			discovery->identity_count);
	zassert_mem_equal(discovery->discovery_vdo,
			this->partner_emul.common_data.identity_vdm + 1,
			discovery->identity_count * sizeof(*discovery->discovery_vdo),
			"Discovered SOP identity ACK did not match");
	zassert_equal(discovery->svid_count, 1, "Expected 1 SVID, got %d",
			discovery->svid_count);
	zassert_equal(discovery->svids[0].svid, USB_SID_DISPLAYPORT,
			"Expected SVID 0x%0000x, got 0x%0000x", USB_SID_DISPLAYPORT,
			discovery->svids[0].svid);
	zassert_equal(discovery->svids[0].mode_count, 1,
			"Expected 1 DP mode, got %d",
			discovery->svids[0].mode_count);
	zassert_equal(discovery->svids[0].mode_vdo[0],
			this->partner_emul.common_data.modes_vdm[1],
			"DP mode VDOs did not match");
}

ZTEST_SUITE(usbc_alt_mode, drivers_predicate_post_main, usbc_alt_mode_setup,
		usbc_alt_mode_before, usbc_alt_mode_after, NULL);
