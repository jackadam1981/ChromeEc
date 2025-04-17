/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_USBC_DP_MODE_INCLUDE_USBC_DP_MODE_H_
#define ZEPHYR_TEST_DRIVERS_USBC_DP_MODE_INCLUDE_USBC_DP_MODE_H_

#include "usb_pd_vdo.h"

#define TEST_PORT USBC_PORT_C0

/* Remove polarity for any mux checks */
#define USB_MUX_CHECK_MASK ~USB_PD_MUX_POLARITY_INVERTED
#define DPAM_VER_VDO(x) (x << 30)

/* Passive cable with USB3 gen 2 speed */
static struct tcpci_cable_data passive_usb3_32 = {
	.identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS(SVDM_VER_2_0),
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

static void set_passive_usb3_32(struct tcpci_cable_data *passive_usb3_32_20,
				int svdm_version)
{
	passive_usb3_32_20->identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS(svdm_version);
}

static void add_dp_discovery(struct tcpci_partner_data *partner,
			     int svdm_version)
{
	/* Add Discover Identity response */
	partner->identity_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_PD, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DISCOVER_IDENT) |
		VDO_SVDM_VERS(svdm_version);
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
		VDO_SVDM_VERS(svdm_version);
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
		VDO_SVDM_VERS(svdm_version);
	partner->svids_vdm[VDO_INDEX_HDR + 1] =
		VDO_SVID(USB_SID_DISPLAYPORT, 0);
	partner->svids_vdos = VDO_INDEX_HDR + 2;
}

static void add_displayport_mode_responses(struct tcpci_partner_data *partner,
					   int svdm_version)
{
	/* Add DisplayPort EnterMode response */
	partner->enter_mode_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_ENTER_MODE) |
		VDO_SVDM_VERS(svdm_version);
	partner->enter_mode_vdos = VDO_INDEX_HDR + 1;

	/* Add DisplayPort StatusUpdate response */
	partner->dp_status_vdm[VDO_INDEX_HDR] =
		VDO(USB_SID_DISPLAYPORT, /* structured VDM */ true,
		    VDO_CMDT(CMDT_RSP_ACK) | CMD_DP_STATUS) |
		VDO_SVDM_VERS(svdm_version);
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
		VDO_SVDM_VERS(svdm_version);
	partner->dp_config_vdos = VDO_INDEX_HDR + 1;
}

static void usbc_dp_mode_before(void *data)
{
	/* Set chipset on so the "AP" is on to give us commands */
	test_set_chipset_to_s0();
}

#endif /* ZEPHYR_TEST_DRIVERS_USBC_DP_MODE_INCLUDE_USBC_DP_MODE_H_ */