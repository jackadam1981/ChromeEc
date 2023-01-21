/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Display Port 2.1 mode header.
 */

#ifndef __CROS_EC_USB_PD_DP_COMPAT_H
#define __CROS_EC_USB_PD_DP_COMPAT_H

#include "usb_pd_vdo.h"

/*
 * SOP' Cable DP Capabilities (Discover Mode)
 * -------------------------------
 * <31:30> : DPAM Version
 * <29:28> : cable type : 0h == Passive, 1h == Active ReTimer
 *                        2h == Active ReDriver, 3h == Optical
 * <27>    : SBZ
 * <26>    : UHBR13.5 Support
 * <25:24> : SBZ
 * <23:16> : UFP_D pin assignment supported
 * <15:8>  : DFP_D pin assignment supported
 * <7:6>   : SBZ
 * <5:2>   : signalling : XXX1b == HBR3, XX1Xb == UHBR10, X1XXb == UHBR20
 *           Other bits are reserved for higher bit rate.
 * <1:0>   : SBZ
 */

union dp_mode_resp_cable {
	struct {
		uint8_t reserved1 : 2;
		uint8_t signaling : 4;
		uint8_t reserved2 : 2;
		uint8_t dfp_d_pin : 8;
		uint8_t ufp_d_pin : 8;
		uint8_t reserved3 : 2;
		uint8_t uhbr13_5_support : 1;
		uint8_t reserved4 : 1;
		uint8_t cable_type : 2;
		uint8_t dpam_ver : 2;
	};
	uint32_t raw_value;
};

enum dpam_version {
	DPAM_VERSION_20 = 0,
	DPAM_VERSION_21,
};

enum dp21_speed {
	DP21_SPEED_HBR3 = 0x1,
	DP21_SPEED_UHBR10 = 0x3,
	DP21_SPEED_UHBR20 = 0x7,
};

enum dp21_cable_type {
	DP21_PASSIVE_CABLE = 0,
	DP21_ACTIVE_RETIMER_CABLE,
	DP21_ACTIVE_REDRIVER_CABLE,
	DP21_OPTICAL_CABLE,
};

/*
 * DisplayPort Configure VDO
 * -------------------------
 * <31:30> : DPAM Version
 * <29:28> : cable type : 0h == Passive, 1h == Active ReTimer
 *                        2h == Active ReDriver, 3h == Optical
 * <27>    : SBZ
 * <26>    : UHBR13.5 Support
 * <25:24> : SBZ
 * <23:16> : UFP_D pin assignment supported
 * <15:8>  : DFP_D pin assignment supported
 * <7:6>   : SBZ
 * <5:2>   : signalling : XXX1b == HBR3, XX1Xb == UHBR10, X1XXb == UHBR20
 *           Other bits are reserved for higher bit rate.
 * <1:0>   : cfg : 00 == USB, 01 == DFP_D, 10 == UFP_D, 11 == reserved
 */
#define VDO_DP2_1_CFG(dpam_version, cable, uhbr13_5, pin, sig, cfg)      \
	(((dpam_version)&0x3) << 30 | ((cable)&0x3) << 28 |              \
	 ((uhbr13_5)&0x1) << 26 | ((pin)&0xff) << 8 | ((sig)&0xf) << 2 | \
	 ((cfg)&0x3))

enum dpam_version resolve_dpam_version(int port, enum tcpci_msg_type type);
enum usb_pd_svdm_ver resolve_svdm_version(int port, enum tcpci_msg_type type);
uint8_t get_dp_cable_bit_rate(int port);
bool dp_mode_entry_allowed(int port);
uint32_t pd_get_dp_mode_vdo(int port, enum tcpci_msg_type type);
#endif /* __CROS_EC_USB_PD_DP_COMPAT_H */
