/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Thunderbolt-compatible mode header.
 */

#ifndef __CROS_EC_USB_PD_TBT_COMPAT_H
#define __CROS_EC_USB_PD_TBT_COMPAT_H

#include "usb_pd_vdo.h"

/*
 * Reference: USB Type-C cable and connector specification, Release 2.0
 */

/*****************************************************************************/
/*
 * TBT3 Passive Cable Discover Identity Responses
 */

/*
 * Table F-1 TBT3 Passive Cable Discover Identity VDO Responses)
 * -------------------------------------------------------------
 * <31>    : USB Communications Capable as USB Host
 *           0b = No
 *           1b = Yes
 * <30>    : USB Communications Capable as a USB Device
 *           0b = No
 *           1b = Yes
 * <29:27> : Cable Product Type (Cable Plug)
 *           011b = Passive
 * <26>    : Modal Operation Supported
 *           0b = No
 *           1b = Yes
 * <25:23> : Product Type
 *           000b - DFP
 * <22:16> : Reserved
 * <15:0>  : Per vendor USB Vendor ID
 */

/*
 * Table F-2 TBT3 Passive Cable VDO for USB PD Revision 2.0, Version 1.3
 * -------------------------------------------------------------
 * <31:28> : 0000b..1111b HW Version
 * <27:24> : 0000b..1111b Firmware Version
 * <23:20> : Reserved
 * <19:18> : Type-C USB Type-C plug to USB TypeC/Captive
 *           10b = USB
 * <17>    : Reserved
 * <16:13> : Cable Latency
 *           0001b – <10ns (~1m)
 *           0010b – 10ns to 20ns (~2m)
 * <12:11> : Cable Termination Type
 *           00b = VCONN not required
 * <10>    : SSTX1 Directionality Support
 * <9>     : SSTX2 Directionality Support
 * <8>     : SSRX1 Directionality Support
 * <7>     : SSRX2 Directionality Support
 * <6:5>   : VBUS Current Handling Capacity
 *           01b = 3A
 *           10b = 5A
 * <4>     : VBUS through cable
 *           0b = No
 *           1b = Yes
 * <3>     : SOP” controller present
 *           0b = No
 *           1b = Yes
 * <2:0>   : SuperSpeed USB Signaling Support
 *           010b = [USB 3.1] Gen1 and Gen2
 */
union tbt_passive_cable_vdo_rev20 {
	struct {
		enum usb_rev20_ss ss: 3;
		uint8_t sop_p_p : 1;
		uint8_t vbus_cable : 1;
		enum usb_vbus_cur vbus_cur : 2;
		uint8_t ssrx2 : 1;
		uint8_t ssrx1 : 1;
		uint8_t sstx2 : 1;
		uint8_t sstx1 : 1;
		uint8_t termination : 2;
		uint8_t latency : 4;
		uint8_t reserved1 : 1;
		uint8_t connector : 2;
		uint8_t reserved2 : 4;
		uint8_t fw_version : 4;
		uint8_t hw_version : 4;
	};
	uint32_t raw_value;
};

/*
 * Table F-3 TBT3 Passive Cable VDO for USB PD Revision 3.0, Version 1.2
 * -------------------------------------------------------------
 * <31:28> : 0000b..1111b HW Version
 * <27:24> : 0000b..1111b Firmware Version
 * <23:21> : VDO Version
 *           000b = Version 1.0
 * <20>    : Reserved
 * <19:18> : Type-C USB Type-C plug to USB TypeC/Captive
 *           10b = USB
 * <17>    : Reserved
 * <16:13> : Cable Latency
 *           0001b – <10ns (~1m)
 *           0010b – 10ns to 20ns (~2m)
 * <12:11> : Cable Termination Type
 *           00b = VCONN not required
 * <10:9>  : Maximum VBUS Voltage
 *           00b = 20V
 * <8:7>   : Reserved
 * <6:5>   : VBUS Current Handling Capacity
 *           01b = 3A
 *           10b = 5A
 * <4:3>   : Reserved
 * <2:0>   : SuperSpeed USB Signaling Support
 *           010b = [USB 3.1] Gen1 and Gen2
 */
union tbt_passive_cable_vdo_rev30 {
	struct {
		enum usb_rev30_ss ss: 3;
		uint8_t reserved0 : 2;
		enum usb_vbus_cur vbus_cur : 2;
		uint8_t reserved1 : 2;
		uint8_t vbus_max : 2;
		uint8_t termination : 2;
		uint8_t latency : 4;
		uint8_t reserved2 : 1;
		uint8_t connector : 2;
		uint8_t reserved3 : 1;
		uint8_t vdo_version : 3;
		uint8_t fw_version : 4;
		uint8_t hw_version : 4;
	};
	uint32_t raw_value;
};

/*****************************************************************************/
/*
 * TBT3 Device Discover Identity Responses
 */

/*
 * Table F-8 TBT3 Device Discover Identity VDO Responses
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
 * TBT3 Discover SVID Responses
 */

/*
 * Table F-9 TBT3 Discover SVID VDO Responses
 * -------------------------------------------------------------
 * Note: These SVID can be in any order
 * <31:16> : 0x8087 = Intel/TBT3 SVID 0
 * <B15:0> : 0xFF01 = VESA DP (if supported) SVID 1
 */

/*****************************************************************************/
/*
 * TBT3 Device Discover Mode Responses
 */

/*
 * Table F-10 TBT3 Device Discover Mode VDO Responses
 * -------------------------------------------------------------
 * <31>    : Vendor specific B1
 *           0b = Not supported
 *           1b = Supported
 * <30>    : Vendor specific B0
 *           0b = Not supported
 *           1b = Supported
 * <29:27> : Reserved
 * <26>    : Intel specific B0
 *           0b = Not supported
 *           1b = Supported
 * <25:17> : Reserved
 * <16>    : TBT Adapter
 *           0b = TBT2 Legacy Adapter
 *           1b = TBT3 Adapter
 * <15:0>  : TBT Alternate Mode
 *           0x0001 = TBT Mode
 */
enum tbt_adapter_type {
	TBT_ADAPTER_TBT2_LEGACY,
	TBT_ADAPTER_TBT3,
};

/* TBT Alternate Mode */
#define TBT_ALTERNATE_MODE 0x0001
#define PD_VDO_RESP_MODE_INTEL_TBT(x)	(((x) & 0xff) == TBT_ALTERNATE_MODE)

union tbt_mode_resp_device {
	struct {
		uint16_t tbt_alt_mode : 16;
		enum tbt_adapter_type tbt_adapter : 1;
		uint16_t reserved0 : 9;
		uint8_t intel_spec_b0 : 1;
		uint8_t reserved1 : 3;
		uint8_t vendor_spec_b0 : 1;
		uint8_t vendor_spec_b1 : 1;
	};
	uint32_t raw_value;
};

/*
 * Table F-11 TBT3 Cable Discover Mode VDO Responses
 * -------------------------------------------------------------
 * <31:24> : Reserved
 * <23>    : Active Cable Plug Link Training
 *           0 = Active with bi-directional LSRX1 communication or when Passive
 *           1 = Active with uni-directional LSRX1 communication
 * <22>    : Re-timer
 *           0 = Not re-timer
 *           1 = Re-timer
 * <21>    : Cable Type
 *           0b = Non-Optical
 *           1b = Optical
 * <20:19> : TBT_Rounded_Support
 *           00b = 3rd Gen Non-Rounded TBT
 *           01b = 3rd & 4th Gen Rounded and Non-Rounded TBT
 *           10b..11b = Reserved
 * <18:16> : Cable Speed
 *           000b = Reserved
 *           001b = USB3.1 Gen1 Cable (10 Gbps TBT support)
 *           010b = 10 Gbps (USB 3.2 Gen1 and Gen2 passive cables)
 *           011b = 10 Gbps and 20 Gbps (TBT 3rd Gen active cables and
 *                  20 Gbps passive cables)
 *           100b..111b = Reserved
 * <15:0>  : TBT Alternate Mode
 *           0x0001 = TBT Mode
 */
enum tbt_compat_cable_speed {
	TBT_SS_RES_0,
	TBT_SS_U31_GEN1,
	TBT_SS_U32_GEN1_GEN2,
	TBT_SS_TBT_GEN3,
	TBT_SS_RES_4,
	TBT_SS_RES_5,
	TBT_SS_RES_6,
	TBT_SS_RES_7,
};

enum tbt_cable_type {
	TBT_CABLE_NON_OPTICAL,
	TBT_CABLE_OPTICAL,
};

enum tbt_compat_rounded_support {
	TBT_GEN3_NON_ROUNDED,
	TBT_GEN3_GEN4_ROUNDED_NON_ROUNDED,
	TBT_ROUND_SUP_RES_2,
	TBT_ROUND_SUP_RES_3,
};

enum usb_retimer_type {
	USB_NOT_RETIMER,
	USB_RETIMER,
};

enum link_lsrx_comm {
	BIDIR_LSRX_COMM,
	UNIDIR_LSRX_COMM,
};

union tbt_mode_resp_cable {
	struct {
		uint16_t tbt_alt_mode : 16;
		enum tbt_compat_cable_speed tbt_cable_speed : 3;
		enum tbt_compat_rounded_support tbt_rounded : 2;
		enum tbt_cable_type tbt_cable : 1;
		enum usb_retimer_type retimer_type : 1;
		enum link_lsrx_comm lsrx_comm : 1;
		uint8_t reserved0 : 8;
	};
	uint32_t raw_value;
};

/*****************************************************************************/
/*
 * TBT3 Enter Mode Command
 */

/*
 * Table F-13 TBT3 Device Enter Mode Command SOP
 * -------------------------------------------------------------
 * <31>    : Vendor specific B1
 *           0 = Not supported
 *           1 = Supported
 * <30>    : Vendor specific B0
 *           0 = Not supported
 *           1 = Supported
 * <29:27> : 000b Reserved
 * <26>    : Intel specific B0
 *           0 = Not supported
 *           1 = Supported
 * <25>    : 0b Reserved
 * <24>    : Active_Passive
 *           0 = Passive cable
 *           1 = Active cable
 * <23>    : Active Cable Link Training
 *           0 = Active with bi-directional LSRX1 communication or when Passive
 *           1 = Active with uni-directional LSRX1 communication
 * <22>    : Re-timer
 *           0 = Not re-timer
 *           1 = Re-timer
 * <21>    : Cable Type
 *           0b = Non-Optical
 *           1b = Optical
 * <20:19> : TBT_Rounded_Support
 *           00b = 3rd Gen Non-Rounded TBT
 *           01b = 3rd & 4th Gen Rounded and Non-Rounded TBT
 *           10b..11b = Reserved
 * <18:16> : Cable Speed
 *           000b = Reserved
 *           001b = USB3.1 Gen1 Cable (10 Gbps TBT support)
 *           010b = 10 Gbps (USB 3.2 Gen1 and Gen2 passive cables)
 *           011b = 10 Gbps and 20 Gbps (TBT 3rd Gen active cables and
 *                  20 Gbps passive cables)
 *           100b..111b = Reserved
 * <15:0>  : TBT Alternate Mode
 *           0x0001 = TBT Mode
 */
enum tbt_enter_cable_type {
	TBT_ENTER_PASSIVE_CABLE,
	TBT_ENTER_ACTIVE_CABLE,
};

union tbt_dev_mode_enter_cmd {
	struct {
		uint16_t tbt_alt_mode : 16;
		enum tbt_compat_cable_speed tbt_cable_speed : 3;
		enum tbt_compat_rounded_support tbt_rounded : 2;
		enum tbt_cable_type tbt_cable : 1;
		enum usb_retimer_type retimer_type : 1;
		enum link_lsrx_comm lsrx_comm : 1;
		enum tbt_enter_cable_type cable : 1;
		uint8_t reserved0 : 1;
		uint8_t intel_spec_b0 : 1;
		uint8_t reserved1 : 3;
		uint8_t vendor_spec_b0 : 1;
		uint8_t vendor_spec_b1 : 1;
	};
	uint32_t raw_value;
};

#endif /* __CROS_EC_USB_PD_TBT_COMPAT_H */
