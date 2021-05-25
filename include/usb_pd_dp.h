/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Thunderbolt-compatible mode header.
 */

#ifndef __CROS_EC_USB_PD_DP_H
#define __CROS_EC_USB_PD_DP_H

#include "usb_pd_vdo.h"

/*
 * NOTE: Throughout the file, some of the bit fields in the structures are for
 * information purpose, they might not be actually used in the current code.
 * When appropriate, replace the bit fields in the structures with appropriate
 * enums.
 */

/*****************************************************************************/
/*
 * DP Device Discover Identity Responses
 */

/*
 * Ref: USB Power Delivery Specification Revision 3.0, Version 2.0
 * Table 6-29 ID Header VDO: Device Discover Identity VDO Responses
 * -------------------------------------------------------------
 * <31>    : USB Communications Capable as USB Host
 *           0b = No
 *           1b = Yes
 * <30>    : USB Communications Capable as a USB Device
 *           0b = No
 *           1b = Yes
 * <29:27> : Product Type (UFP)
 *           001b = PDUSB Hub
 *           010b = PDUSB Peripheral
 *           101b = Alternate Mode Adapter (AMA)
 *           110b = VCONN-Powered USB Device (VPD)
 * <26>    : Modal Operation Supported Modal Operation Supported
 *           0b = No
 *           1b = Yes
 * <25:23> : Product Type (DFP)
 *           001b = PDUSB Hub
 *           010b = PDUSB Host
 *           100b = Alternate Mode Controller (AMC)
 * <22:16> : 0 Reserved
 * <15:0>  : Per vendor USB Vendor ID
 */

/*****************************************************************************/
/*
 * DP Discover SVID Responses
 */

/*
 * Ref: USB Power Delivery Specification Revision 3.0, Version 2.0
 * Table 6-43 Discover SVIDs Responder VDO
 * -------------------------------------------------------------
 * <B15:0> : 0xFF01 = VESA DP (if supported) SVID 1
 */

/*****************************************************************************/
/*
 * DP Device Discover Mode Responses
 */

/*
 * -------------------------------
 * <31:24> : Reserved
 * <23:16> : UFP_D pin assignment supported
 *           00000000b = UFP_D pin assignments are not supported
 *           xxxxxxx1b = reserved
 *           xxxxxx1xb = reserved.
 *           xxxxx1xxb = Pin Assignment C is supported.
 *           xxxx1xxxb = Pin Assignment D is supported.
 *           xxx1xxxxb = Pin Assignment E is supported.
 *           xx1xxxxxb = reserved.
 *           x1xxxxxxb = reserved.
 *           1xxxxxxxb = reserved.
 * <15:8>  : DFP_D pin assignment supported
 *           00000000b = UFP_D pin assignments are not supported
 *           xxxxxxx1b = reserved
 *           xxxxxx1xb = reserved.
 *           xxxxx1xxb = Pin Assignment C is supported.
 *           xxxx1xxxb = Pin Assignment D is supported.
 *           xxx1xxxxb = Pin Assignment E is supported.
 *           xx1xxxxxb = reserved.
 *           x1xxxxxxb = reserved.
 *           1xxxxxxxb = reserved.
 * <7>     : USB 2.0 signaling not used
 *           0b = May be required
 *           1b = Not required
 * <6>     : Receptacle indication
 *           0b = DP interface is presented on a USB Type-C plug.
 *           1b = DP interface is presented on a USB Type-C receptacle.
 * <5:2>   : Signaling for Transport of DisplayPort Protocol
 *           xxx1b =  Supports standard signaling
 *           xx1xb = reserved
 *           x1xxb = reserved
 *           1xxxb = reserved
 * <1:0>   : Port Capability
 *           00b = reserved
 *           01b = UFP_D-capable.
 *           10b = DFP_D-capable.
 *           11b = Both DFP_D and UFP_D-capable.
 */
#define VDO_MODE_DP(snkp, srcp, usb, gdr, sign, sdir)			\
	(((snkp) & 0xff) << 16 | ((srcp) & 0xff) << 8			\
	 | ((usb) & 1) << 7 | ((gdr) & 1) << 6 | ((sign) & 0xF) << 2	\
	 | ((sdir) & 0x3))

/* Pin configs B/D/F support multi-function */
#define MODE_DP_PIN_MF_MASK 0x2a
/* Pin configs A/B support BR2 signaling levels */
#define MODE_DP_PIN_BR2_MASK 0x3
/* Pin configs C/D/E/F support DP signaling levels */
#define MODE_DP_PIN_DP_MASK 0x3c
/* Pin configs A/B/C/D/E/F */
#define MODE_DP_PIN_CAPS_MASK 0x3f

enum dp_signaling {
	DP_SIGNALING_STANDARD = BIT(0),
	DP_SIGNALING_RESERVED0 = BIT(1),
	DP_SIGNALING_RESERVED1 = BIT(2),
	DP_SIGNALING_RESERVED2 = BIT(3),
};

enum dp_port_capability {
	DP_PORT_CAP_RESV,
	DP_PORT_CAP_UFP_D,
	DP_PORT_CAP_DFP_D,
	DP_PORT_CAP_BOTH,
};

enum dp_usb_typec_indication {
	DP_USB_TYPEC_INDICATION_PLUG,
	DP_USB_TYPEC_INDICATION_RECEPTACLE,
};

enum dp_pin_assignment {
	PIN_ASGMT_NOT_SUPPORTED = 0,
	PIN_ASGMT_A = BIT(0),
	PIN_ASGMT_B = BIT(1),
	PIN_ASGMT_C = BIT(2),
	PIN_ASGMT_D = BIT(3),
	PIN_ASGMT_E = BIT(4),
	PIN_ASGMT_F = BIT(5),
	PIN_ASGMT_RESERVED3 = BIT(6),
	PIN_ASGMT_RESERVED4 = BIT(7),
};

union dp_mode_resp_device {
	struct {
		enum dp_port_capability port_cap : 2;
		enum dp_signaling signaling : 4;
		enum dp_usb_typec_indication typec_indication : 1;
		uint8_t usb2_signaling : 1;
		enum dp_pin_assignment dfp_d_pin_assign : 8;
		enum dp_pin_assignment ufp_d_pin_assign : 8;
		uint8_t reserved : 8;
	};
	uint32_t raw_value;
};

/*
 * Determine which pin assignments are valid for DP
 *
 * Based on whether the DP adapter identifies itself as a plug (permanently
 * attached cable) or a receptacle, the pin assignments may be in the DFP_D
 * field or the UFP_D field.
 *
 * Refer to DisplayPort Alt Mode On USB Type-C Standard version 1.0, table 5-2
 * depending on state of receptacle bit, use pins for DFP_D (if receptacle==0)
 * or UFP_D (if receptacle==1)
 * Also refer to DisplayPort Alt Mode Capabilities Clarification (4/30/2015)
 */
#define PD_DP_PIN_CAPS(x) ((((x) >> MODE_DP_CABLE_SHIFT) & 0x1) \
	? (((x) >> MODE_DP_UFP_PIN_SHIFT) & MODE_DP_PIN_CAPS_MASK) \
	: (((x) >> MODE_DP_DFP_PIN_SHIFT) & MODE_DP_PIN_CAPS_MASK))

/*
 * DisplayPort Status VDO
 * ----------------------
 * <31:9> : Reserved.
 * <8>    : IRQ_HPD.
 *          0b = No IRQ_HPD since last status message.
 *          1b = IRQ_HPD.
 * <7>    : HPD state.
 *          0b = HPD_LOW.
 *          1b = HPD_HIGH.
 * <6>    : Exit DisplayPort Mode Request.
 *          0b = Maintain current mode.
 *          1b = Request exit from DisplayPort Mode.
 * <5>    : USB configuration Request.
 *          0b = maintain current configuration.
 *          1b = switch to USB configuration.
 * <4>    : Multi-function preference.
 *          0b = No preference.
 *          1b = Multi-function preferred.
 * <3>    : Enabled.
 *          0b = Adapter DP functionality is disabled.
 *          1b = Adapter DP functionality is enabled and operational.
 * <2>    : Power low
 *          0b = Adapter is functioning normally or is disabled.
 *          1b = Adapter has detected low power and disabled DP support.
 * <1:0>  : DFP_D/UFP_D Connected.
 *          00b = Neither DFP_D nor UFP_D is connected, or adapter is disabled.
 *          01b = DFP_D is connected.
 *          10b = UFP_D is connected.
 *          11b = Both DFP_D and UFP_D are connected.
 */
#define VDO_DP_STATUS(irq, lvl, amode, usbc, mf, en, lp, conn)		\
	(((irq) & 1) << 8 | ((lvl) & 1) << 7 | ((amode) & 1) << 6	\
	 | ((usbc) & 1) << 5 | ((mf) & 1) << 4 | ((en) & 1) << 3	\
	 | ((lp) & 1) << 2 | ((conn & 0x3) << 0))

#define PD_VDO_DPSTS_MF_MASK BIT(4)

enum dp_multifunction_pref {
	DP_MF_NO_PREF,
	DP_MF_PREF,
};

enum dp_hpd_state {
	DP_HPD_LOW,
	DP_HPD_HIGH,
};

enum dp_irq_hpd {
	DP_NO_IRQ_HPD_RECV,
	DP_IQR_HPD_RECV,
};

union dp_mode_status_vdo {
	struct {
		uint8_t dp_connection_status : 2;
		uint8_t dp_power_low : 1;
		uint8_t dp_enable : 1;
		enum dp_multifunction_pref mf_pref : 1;
		uint8_t dp_usb_config_req : 1;
		uint8_t dp_exit_mode_req : 1;
		enum dp_hpd_state hpd_state : 1;
		enum dp_irq_hpd irq_hpd : 1;
		uint32_t reserved : 23;
	};
	uint32_t raw_value;
};

/*
 * DisplayPort Configure VDO
 * -------------------------
 * <31:24> : Reserved
 * <23:16> : Reserved
 * <15:8>  : Configure UFP_U Pin Assignment
 *           00000000b = De-select pin assignment.
 *           00000100b = Select Pin Assignment C.
 *           00001000b = Select Pin Assignment D.
 *           00010000b = Select Pin Assignment E.
 *           All other values are Reserved.
 * <7:6>   : Reserved
 * <5:2>   : Signaling for Transport of DisplayPort Protocol
 *           0000b = Signaling unspecified.
 *           0001b = Select DP Standard signaling rates and electrical settings.
 *           All other values are Reserved
 * <1:0>   : Select Configuration
 *           00b = Set configuration for USB.
 *           01b = Set configuration for UFP_U as DFP_D.
 *           10b = Set configuration for UFP_U as UFP_D.
 *           11b = Reserved.
 */
#define VDO_DP_CFG(pin, sig, cfg) \
	(((pin) & 0xff) << 8 | ((sig) & 0xf) << 2 | ((cfg) & 0x3))

enum dp_select_config {
	DP_SET_CONFIG_USB,
	DP_SET_CONFIG_DFP_D,
	DP_SET_CONFIG_UFP_D,
	DP_SET_CONFIG_RESERVED,
};

union dp_configure_vdo {
	struct {
		enum dp_select_config sel_config : 2;
		uint8_t dp_signaling_transport : 4;
		uint8_t reserved0 : 2;
		enum dp_pin_assignment pin_assign : 8;
		uint8_t reserved1 : 8;
		uint8_t reserved2  : 8;
	};
	uint32_t raw_value;
};
#endif
