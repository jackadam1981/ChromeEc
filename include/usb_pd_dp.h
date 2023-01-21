/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Display Port 2.1 mode header.
 */

#ifndef __CROS_EC_USB_PD_DP_COMPAT_H
#define __CROS_EC_USB_PD_DP_COMPAT_H

#include "usb_pd_vdo.h"

/* Reference: VESA DisplayPort Alt Mode on USB Type-C Standard Version 2.1 */

/*
 * Table 4-4: SOP' Cable DP Capabilities
 * ------------------------------------------------------------------
 * <31:30> : DPAM Version
 * <29:28> : cable type : 0h == Passive, 1h == Active ReTimer
 *                        2h == Active ReDriver, 3h == Optical
 * <27>    : reserved
 * <26>    : UHBR13.5 Support
 * <25:24> : reserved
 * <23:16> : UFP_D pin assignment supported
 * <15:8>  : DFP_D pin assignment supported
 * <7:6>   : reserved
 * <5:2>   : signaling : XXX1b == HBR3, XX1Xb == UHBR10, X1XXb == UHBR20
 *           Other bits are reserved for higher bit rate.
 * <1:0>   : reserved
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
		uint8_t active_comp : 2;
		uint8_t dpam_ver : 2;
	};
	uint32_t raw_value;
};

enum dpam_version {
	DPAM_VERSION_20,
	DPAM_VERSION_21,
};

enum dp21_speed {
	DP21_SPEED_HBR3 = 0x1,
	DP21_SPEED_UHBR10 = 0x2,
	DP21_SPEED_UHBR20 = 0x4,
};

enum dp21_cable_type {
	DP21_PASSIVE_CABLE = 0,
	DP21_ACTIVE_RETIMER_CABLE,
	DP21_ACTIVE_REDRIVER_CABLE,
	DP21_OPTICAL_CABLE,
};

/*
 * Table 5-13: SOP DisplayPort Configurations
 * ------------------------------------------------------------------
 * <31:30> : DPAM Version
 * <29:28> : cable type : 0h == Passive, 1h == Active ReTimer
 *                        2h == Active ReDriver, 3h == Optical
 * <27>    : reserved
 * <26>    : UHBR13.5 Support
 * <25:24> : reserved
 * <23:16> : UFP_D pin assignment supported
 * <15:8>  : DFP_D pin assignment supported
 * <7:6>   : reserved
 * <5:2>   : signaling : XXX1b == HBR3, XX1Xb == UHBR10, X1XXb == UHBR20
 *           Other bits are reserved for higher bit rate.
 * <1:0>   : cfg : 00 == USB, 01 == DFP_D, 10 == UFP_D, 11 == reserved
 */
#define VDO_DP2_1_CFG(dpam_version, cable, uhbr13_5, pin, sig, cfg)      \
	(((dpam_version)&0x3) << 30 | ((cable)&0x3) << 28 |              \
	 ((uhbr13_5)&0x1) << 26 | ((pin)&0xff) << 8 | ((sig)&0xf) << 2 | \
	 ((cfg)&0x3))

#define VDM_VERS_MINOR \
	IS_ENABLED(CONFIG_USB_PD_DP21_MODE) ? VDO_SVDM_VERS_MINOR(1) : 0

/* Active/Passive Cable */
#define USB_DP_ACTIVE_CABLE BIT(0)
/* Re-timer/Re-Driver cable */
#define USB_DP_RETIMER_CABLE BIT(1)
/* Optical/Non-optical cable */
#define USB_DP_OPTICAL_CABLE BIT(2)

/**
 * Resolves DPAM version
 *
 * @param port	The PD port number
 * @param type	Transmit type (SOP, SOP') for VDM
 * @return	DPAM_VERSION_20/DPAM_VERSION_21
 */
enum dpam_version resolve_dpam_version(int port, enum tcpci_msg_type type);

/**
 * Resolves SVDM version version
 *
 * @param port	The PD port number
 * @param type	Transmit type (SOP, SOP') for VDM
 * @return	SVDM_VER_2_0/SVDM_VER_2_1
 */
enum usb_pd_svdm_ver resolve_svdm_version(int port, enum tcpci_msg_type type);

/**
 * Get Cable speed
 *
 * @param port	The PD port number
 * @return	cable speed
 */
uint8_t get_dp_cable_bit_rate(int port);

/**
 * Check DP2.1 Mode entry allowed
 *
 * @param port	The PD port number
 * @return	true/false
 */
bool dp21_mode_entry_allowed(int port);

/**
 * Get Mode VDO data for DisplayPort svid
 *
 * @param port	The PD port number
 * @param type	Transmit type (SOP, SOP') for VDM
 * @return	Mode VDO
 */
uint32_t pd_get_dp_mode_vdo(int port, enum tcpci_msg_type type);

/**
 * Combines the following information into a single byte
 * Bit 0: Active/Passive cable
 * Bit 1: Retimer/Redriver cable
 * Bit 2: Optical/Non-optical cable
 *
 * @param port	The PD port number
 * @return flags
 */
uint8_t get_pd_cable_type_flags(int port);

/**
 * Get uhbr13.5 is supported
 *
 * @param port	The PD port number
 * @return	0 - UHBR13.5 Not supported, 1 - UHBR13.5 Supported
 */
__overridable uint8_t is_dp_uhbr13_5_supported(int port);
#endif /* __CROS_EC_USB_PD_DP_COMPAT_H */
