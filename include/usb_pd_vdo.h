/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB-PD Cable type header.
 */

#ifndef __CROS_EC_USB_PD_VDO_H
#define __CROS_EC_USB_PD_VDO_H

/*
 * ############################################################################
 *
 * Reference: USB Power Delivery Specification Revision 3.0, Version 2.0
 *
 * ############################################################################
 */

/*****************************************************************************/
/*
 * Table 6-35 UFP VDO 1
 * -------------------------------------------------------------
 * <31:29> : UFP VDO version
 * <28>    : Reserved
 * <27:24> : Device Capability
 * <23:6>  : Reserved
 * <5:3>   : Alternate Modes
 * <2:0>   : USB Highest Speed
 */
#define PD_PRODUCT_IS_USB4(vdo) ((vdo) >> 27 & 0x1)

/*****************************************************************************/
/*
 * Table 6-38 Passive Cable VDO
 * -------------------------------------------------------------
 * <31:28> : HW Version
 *           0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version
 *           0000b..1111b assigned by the VID owner
 * <23:21> : VDO Version
 *           Version Number of the VDO (not this specification Version):
 •           Version 1.0 = 000b
 *           Values 001b..111b are Reserved and Shall Not be used
 * <20>    : Reserved
 *           Shall be set to zero.
 * <19:18> : USB Type-C plug to USB TypeC/Captive
 *           00b = Reserved, Shall Not be used
 *           01b = Reserved, Shall Not be used
 *           10b = USB Type-C
 *           11b = Captive
 * <17>    : Reserved
 *           Shall be set to zero.
 * <16:13> : Cable Latency
 *           0000b – Reserved, Shall Not be used
 *           0001b – <10ns (~1m)
 *           0010b – 10ns to 20ns (~2m)
 *           0011b – 20ns to 30ns (~3m)
 *           0100b – 30ns to 40ns (~4m)
 *           0101b – 40ns to 50ns (~5m)
 *           0110b – 50ns to 60ns (~6m)
 *           0111b – 60ns to 70ns (~7m)
 *           1000b – > 70ns (>~7m)
 *           1001b..1111b Reserved, Shall Not be used
 *           Includes latency of electronics in Active Cable
 * <12:11> : Cable Termination Type
 *           00b = VCONN not required.
 *                 Cable Plugs that only support Discover Identity Commands
 *                 Shall set these bits to 00b.
 *           01b = VCONN required
 *           10b..11b = Reserved, Shall Not be used
 * <10:9>  : Maximum VBUS Voltage
 *           00b – 20V
 *           01b – 30V
 *           10b – 40V
 *           11b – 50V
 * <8:7>   : Reserved
 *           Shall be set to zero.
 * <6:5>   : VBUS Current Handling Capability
 *           00b = Reserved, Shall Not be used.
 *           01b = 3A
 *           10b = 5A
 *           11b = Reserved, Shall Not be used.
 * <4:3>   : Reserved
 *           Shall be set to zero.
 * <2:0>   : USB Highest Speed
 *           000b = [USB 2.0] only, no SuperSpeed support
 *           001b = [USB 3.2] Gen1
 *           010b = [USB 3.2]/[USB4] Gen2
 *           011b = [USB4] Gen3
 *           100b..111b = Reserved, Shall Not be used
 */

enum usb_rev30_ss {
	USB_R30_SS_U2_ONLY,
	USB_R30_SS_U32_GEN1,
	USB_R30_SS_U32_USB40_GEN2,
	USB_R30_SS_U40_GEN3,
	USB_R30_SS_RES_4,
	USB_R30_SS_RES_5,
	USB_R30_SS_RES_6,
	USB_R30_SS_RES_7,
};

enum usb_vbus_cur {
	USB_VBUS_CUR_RES_0,
	USB_VBUS_CUR_3A,
	USB_VBUS_CUR_5A,
	USB_VBUS_CUR_RES_3,
};

union passive_cable_vdo_rev30 {
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
 * Table 6-39 Active Cable VDO 1
 * -------------------------------------------------------------
 * <31:28> : HW Version 0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version 0000b..1111b assigned by the VID owner
 * <23:21> : VDO Version Version Number of the VDO
 *           (not this specification Version):
 •           Version 1.3 = 011b
 *           Values 000b, 100b..111b are Reserved and Shall Not be used
 * <20>    : Reserved Shall be set to zero.
 * <19:18> : Connector Type 00b = Reserved, Shall Not be used
 *           01b = Reserved, Shall Not be used
 *           10b = USB Type-C
 *           11b = Captive
 * <17>    : Reserved Shall be set to zero.
 * <16:13> : Cable Latency 0000b – Reserved, Shall Not be used
 *           0001b – <10ns (~1m)
 *           0010b – 10ns to 20ns (~2m)
 *           0011b – 20ns to 30ns (~3m)
 *           0100b – 30ns to 40ns (~4m)
 *           0101b – 40ns to 50ns (~5m)
 *           0110b – 50ns to 60ns (~6m)
 *           0111b – 60ns to 70ns (~7m)
 *           1000b –1000ns (~100m)
 *           1001b –2000ns (~200m)
 *           1010b – 3000ns (~300m)
 *           1011b..1111b Reserved, Shall Not be used
 *           Includes latency of electronics in Active Cable
 * <12:11> : Cable Termination Type 00b..01b = Reserved, Shall Not be used
 *           10b = One end Active, one end passive, VCONN required
 *           11b = Both ends Active, VCONN required
 * <10:9>  : Maximum VBUS Voltage Maximum Cable VBUS Voltage:
 *           00b – 20V
 *           01b – 30V
 *           10b – 40V
 *           11b – 50V
 * <8>     : SBU Supported
 *           0 = SBUs connections supported
 *           1 = SBU connections are not supported
 * <7>     : SBU Type
 *           When SBU Supported = 1 this bit Shall be Ignored
 *           When SBU Supported = 0:
 *           0 = SBU is passive
 *           1 = SBU is active
 * <6:5>   : VBUS Current Handling Capability
 *           When VBUS Through Cable is “No”, this field Shall be Ignored.
 *           When VBUS Though Cable is “Yes”:
 *           00b = USB Type-C Default Current
 *           01b = 3A
 *           10b = 5A
 *           11b = Reserved, Shall Not be used.
 * <4>     : VBUS Through Cable
 *           0 = No
 *           1 = Yes
 * <3>     : SOP” Controller Present
 *           0 = No SOP” controller present
 *           1 = SOP” controller present
 * <2:0>   : USB Highest Speed
 *           000b = [USB 2.0] only, no SuperSpeed support
 *           001b = [USB 3.2] Gen1
 *           010b = [USB 3.2]/ [USB4] Gen2
 *           011b = [USB4] Gen3
 *           100b..111b = Reserved, Shall Not be used
 */
union active_cable_vdo1_rev30 {
	struct {
		enum usb_rev30_ss ss: 3;
		uint8_t sop_p_p : 1;
		uint8_t vbus_cable : 1;
		enum usb_vbus_cur vbus_cur : 2;
		uint8_t sbu_type : 1;
		uint8_t sbu_support : 1;
		uint8_t vbus_max : 2;
		uint8_t termination : 2;
		uint8_t latency : 4;
		uint8_t reserved0 : 1;
		uint8_t connector : 2;
		uint8_t reserved1 : 1;
		uint8_t vdo_version : 3;
		uint8_t fw_version : 4;
		uint8_t hw_version : 4;
	};
	uint32_t raw_value;
};


/*****************************************************************************/
/*
 * Table 6-40 Active Cable VDO 2
 * -------------------------------------------------------------
 * <31:24> : Maximum Operating Temperature
 *           The maximum internal operating temperature.
 *           It may or may not reflect the plug’s skin temperature.
 * <23:16> : Shutdown Temperature
 *           The temperature at which the cable will go into thermal shutdown
 *           so as not to exceed the allowable plug skin temperature.
 * <15>    : Reserved Shall be set to zero.
 * <14:12> : U3/CLd Power 000b: >10mW
 *           001b: 5-10mW
 *           010b: 1-5mW
 *           011b: 0.5-1mW
 *           100b: 0.2-0.5mW
 *           101b: 50-200µW
 *           110b: <50µW
 *           111b: Reserved, Shall Not be used
 * <11>    : U3 to U0 transition mode
 *           0b: U3 to U0 direct
 *           1b: U3 to U0 through U3S
 * <10>    : Physical connection
 *           0b = Copper
 *           1b = Optical
 * <9>     : Active element
 *           0b = Active Redriver
 *           1b = Active Retimer
 * <8>     : USB4 Supported
 *           0b = [USB4] supported
 *           1b = [USB4] not supported
 * <7:6>   : USB 2.0 Hub Hops Consumed
 *           Number of [USB 2.0] ‘hub hops’ cable consumes.
 *           Shall be set to 0 if USB 2.0 not supported.
 * <5>     : USB 2.0 Supported
 *           0b = [USB 2.0] supported
 *           1b = [USB 2.0] not supported
 * <4>     : USB 3.2 Supported
 *           0b = [USB 3.2] SuperSpeed supported
 *           1b = [USB 3.2] SuperSpeed not supported
 * <3>     : USB Lanes Supported
 *           0b = One lane
 *           1b = Two lanes
 * <2>     : Optically Isolated Active Cable
 *           0b = No
 *           1b = Yes
 * <1>     : Reserved Shall be set to zero.
 * <0>     : USB Gen
 *           0b = Gen 1
 *           1b = Gen 2 or higher
 *           Note: see VDO1 USB Highest Speed for details of Gen supported.
 */
union active_cable_vdo2_rev30 {
	struct {
		uint8_t usb_gen : 1;
		uint8_t reserved0 : 1;
		uint8_t a_cable_type : 1;
		uint8_t usb_lanes : 1;
		uint8_t usb_32_support : 1;
		uint8_t usb_20_support : 1;
		uint8_t usb_20_hub_hop : 2;
		uint8_t usb_40_support : 1;
		uint8_t active_elem : 1;
		uint8_t physical_conn : 1;
		uint8_t u3_to_u0 : 1;
		uint8_t u3_power : 3;
		uint8_t reserved1 : 1;
		uint8_t shutdown_temp : 8;
		uint8_t max_operating_temp : 8;
	};
	uint32_t raw_value;
};


/*****************************************************************************/
/*
 * Table 6-41 AMA VDO
 * -------------------------------------------------------------
 * <31:28> : HW Version
 *           0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version
 *           0000b..1111b assigned by the VID owner
 * <23:21> : VDO Version
 *           Version Number of the VDO (not this specification Version):
 *           Version 1.0 = 000b
 *           Values 001b..111b are Reserved and Shall Not be used
 * <20:8>  : Reserved. Shall be set to zero.
 * <7:5>   : VCONN power
 *           When the VCONN required field is set to “Yes” VCONN power
 *           needed by adapter for full functionality
 *           000b = 1W
 *           001b = 1.5W
 *           010b = 2W
 *           011b = 3W
 *           100b = 4W
 *           101b = 5W
 *           110b = 6W
 *           111b = Reserved, Shall Not be used
 *           When the VCONN required field is set to “No” Reserved,
 *           Shall be set to zero.
 * <4>     : VCONN required
 *           0 = No
 *           1 = Yes
 * <3>     : VBUS required
 *           0 = No
 *           1 = Yes
 * <2:0>   : USB Highest Speed
 *           000b = [USB 2.0] only
 *           001b = [USB 3.2] Gen1 and USB 2.0
 *           010b = [USB 3.2] Gen1, Gen2 and USB 2.0
 *           011b = [USB 2.0] billboard only
 *           100b..111b = Reserved, Shall Not be used
 */

/*****************************************************************************/
/*
 * Table 6-42 VPD VDO
 * -------------------------------------------------------------
 * <31:28> : HW Version
 *           0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version
 *           0000b..1111b assigned by the VID owner
 * <23:21> : VDO Version Version Number of the VDO
 *           (not this specification Version):
 *           Version 1.0 = 000b
 *           Values 001b…111b are Reserved and Shall Not be used
 * <20:17> : Reserved Shall be set to zero.
 * <16:15> : Maximum VBUS Voltage
 *           Maximum Cable VBUS Voltage:
 *           00b – 20V
 *           01b – 30V
 *           10b – 40V
 *           11b – 50V
 * <14>    : Charge Through Current Support
 *           Charge Through Support bit=1b:
 *           0b - 3A capable;
 *           1b - 5A capable
 *           Charge Through Support bit = 0b: Reserved, Shall be set to zero
 * <14:13> : Reserved Shall be set to zero.
 * <12:7>  : VBUS Impedance
 *           Charge Through Support bit = 1b:
 *           Vbus impedance through the VPD in 2 mΩ increments.
 *           Values less than 10 mΩ are Reserved and Shall Not be used.
 *           Charge Through Support bit = 0b: Reserved, Shall be set to zero
 * <6:1>   : Ground Impedance
 *           Charge Through Support bit = 1b:
 *           Ground impedance through the VPD in 1 mΩ increments.
 *           Values less than 10 mΩ are Reserved and Shall Not be used.
 *           Charge Through Support bit = 0b: Reserved, Shall be set to zero
 * <0>     : Charge Through Support
 *           1b – the VPD supports Charge Through
 *           0b – the VPD does not support Charge Through
 */

/*
 * ############################################################################
 *
 * Reference: USB Power Delivery Specification Revision 2.0, Version 1.3
 *
 * ############################################################################
 */

/*****************************************************************************/
/*
 * Table 6-28 Passive Cable VDO
 * -------------------------------------------------------------
 * <31:28> : HW Version
 *           0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version
 *           0000b..1111b assigned by the VID owner
 * <23:20> : Reserved
 *           Shall be set to zero.
 * <19:18> : USB Type-C plug to USB Type-A/B/C/Captive
 *           00b = USB Type-A
 *           01b = USB Type-B
 *           10b = USB Type-C
 *           11b = Captive
 * <17>    : Reserved
 *           Shall be set to zero.
 * <16:13> : Cable Latency
 *           0000b – Reserved, Shall Not be used
 *           0001b – <10ns (~1m)
 *           0010b – 10ns to 20ns (~2m)
 *           0011b – 20ns to 30ns (~3m)
 *           0100b – 30ns to 40ns (~4m)
 *           0101b – 40ns to 50ns (~5m)
 *           0110b – 50ns to 60ns (~6m)
 *           0111b – 60ns to 70ns (~7m)
 *           1000b – > 70ns (>~7m)
 *           1001b..1111b Reserved, Shall Not be used
 *           Includes latency of electronics in Active Cable
 * <12:11> : Cable Termination Type
 *           00b = VCONN not required. Cable Plugs that only support
 *                 Discover Identity Commands Shall set these bits to 00b.
 *           01b = VCONN required
 *           10b..11b = Reserved, Shall Not be used
 * <10>    : SSTX1 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <9>     : SSTX2 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <8>     : SSRX1 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <7>     : SSRX2 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <6:5>   : VBUS Current Handling Capability
 *           00b = Reserved, Shall Not be used.
 *           01b = 3A
 *           10b = 5A
 *           11b = Reserved, Shall Not be used.
 * <4>     : VBUS through cable
 *           0 = No
 *           1 = Yes
 * <3>     : Reserved Shall be set to 0.
 * <2:0>   : USB SuperSpeed Signaling Support
 *           000b = USB 2.0 only, no SuperSpeed support
 *           001b = [USB 3.1] Gen1
 *           010b = [USB 3.1] Gen1 and Gen2
 *           011b..111b = Reserved, Shall Not be used
 *           See [USB Type-C 1.2] for definitions.
 */
enum usb_rev20_ss {
	USB_R20_SS_U2_ONLY,
	USB_R20_SS_U31_GEN1,
	USB_R20_SS_U31_GEN1_GEN2,
	USB_R20_SS_RES_3,
	USB_R20_SS_RES_4,
	USB_R20_SS_RES_5,
	USB_R20_SS_RES_6,
	USB_R20_SS_RES_7,
};

union passive_cable_vdo_rev20 {
	struct {
		enum usb_rev20_ss ss: 3;
		uint8_t reserved0 : 1;
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

/*****************************************************************************/
/*
 * Table 6-29 Active Cable VDO
 * -------------------------------------------------------------
 * <31:28> : HW Version
 *           0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version
 *           0000b..1111b assigned by the VID owner
 * <23:20> : Reserved
 *           Shall be set to zero.
 * <19:18> : USB Type-C plug to USB Type-A/B/C/Captive
 *           00b = USB Type-A
 *           01b = USB Type-B
 *           10b = USB Type-C
 *           11b = Captive
 * <17>    : Reserved
 *           Shall be set to zero.
 * <16:13> : Cable Latency
 *           0000b – Reserved, Shall Not be used
 *           0001b – <10ns (~1m)
 *           0010b – 10ns to 20ns (~2m)
 *           0011b – 20ns to 30ns (~3m)
 *           0100b – 30ns to 40ns (~4m)
 *           0101b – 40ns to 50ns (~5m)
 *           0110b – 50ns to 60ns (~6m)
 *           0111b – 60ns to 70ns (~7m)
 *           1000b – > 70ns (>~7m)
 *           1001b..1111b Reserved, Shall Not be used
 *           Includes latency of electronics in Active Cable
 * <12:11> : Cable Termination Type
 *           00b..01b = Reserved, Shall Not be used
 *           10b = One end Active, one end passive, VCONN required
 *           11b = Both ends Active, VCONN required
 * <10>    : SSTX1 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <9>     : SSTX2 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <8>     : SSRX1 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <7>     : SSRX2 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <6:5>   : VBUS Current Handling Capability
 *           00b = Reserved, Shall Not be used.
 *           01b = 3A
 *           10b = 5A
 *           11b = Reserved, Shall Not be used.
 * <4>     : VBUS through cable
 *           0 = No
 *           1 = Yes
 * <3>     : SOP” controller present?
 *           1 = SOP” controller present
 *           0 = No SOP” controller present
 * <2:0>   : USB SuperSpeed Signaling Support
 *           000b = USB 2.0 only, no SuperSpeed support
 *           001b = [USB 3.1] Gen1
 *           010b = [USB 3.1] Gen1 and Gen2
 *           011b..111b = Reserved, Shall Not be used
 */
union active_cable_vdo_rev20 {
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
		uint8_t reserved0 : 1;
		uint8_t connector : 2;
		uint8_t reserved1 : 1;
		uint8_t vdo_version : 3;
		uint8_t fw_version : 4;
		uint8_t hw_version : 4;
	};
	uint32_t raw_value;
};

/*****************************************************************************/
/*
 * Table 6-30 AMA VDO
 * -------------------------------------------------------------
 * <31:28> : HW Version
 *           0000b..1111b assigned by the VID owner
 * <27:24> : Firmware Version
 *           0000b..1111b assigned by the VID owner
 * <23:12> : Reserved
 *           Shall be set to zero.
 * <11>    : SSTX1 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <10>     : SSTX2 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <9>     : SSRX1 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <8>     : SSRX2 Directionality Support
 *           0 = Fixed
 *           1 = Configurable
 * <7:5>   : VCONN power
 *           When the VCONN required field is set to “Yes” VCONN power
 *           needed by adapter for full functionality
 *           000b = 1W
 *           001b = 1.5W
 *           010b = 2W
 *           011b = 3W
 *           100b = 4W
 *           101b = 5W
 *           110b = 6W
 *           111b = Reserved, Shall Not be used
 *           When the VCONN required field is set to “No” Reserved,
 *           Shall be set to zero.
 * <4>     : VCONN required
 *           0 = No
 *           1 = Yes
 * <3>     : VBUS required
 *           0 = No
 *           1 = Yes
 * <2:0>   : USB Highest Speed
 *           000b = [USB 2.0] only
 *           001b = [USB 3.2] Gen1 and USB 2.0
 *           010b = [USB 3.2] Gen1, Gen2 and USB 2.0
 *           011b = [USB 2.0] billboard only
 *           100b..111b = Reserved, Shall Not be used
 */

/*
 * Enter USB Data Object (Ref: USB PD 3.2 Version 2.0 Table 6-47)
 * -----------------------
 * <31>    : Reserved
 * <30:28> : USB Mode
 *           000b - USB2.0
 *           001b - USB3.2
 *           010b - USB4
 * <27>    : Reserved
 * <26>    : USB4 DRD
 *           0b: Not capable of operating as a [USB4] Device
 *           1b: Capable of operating as a [USB4] Device
 * <25>    : USB3 DRD
 *           0b: Not capable of operating as a [USB 3.2] Device
 *           1b: Capable of operating as a [USB 3.2] Device
 * <24>    : Reserved
 * <23:21> : Cable Speed
 *           000b - [USB 2.0] only, no SuperSpeed support
 *           001b - [USB 3.2] Gen1
 *           010b - [USB 3.2] Gen2 and [USB4] Gen2
 *           011b - [USB4] Gen3
 *           111b..100b: Reserved, Shall not be used
 * <20:19> : Cable Type
 *           00b - Passive
 *           01b - Active Re-timer
 *           10b - Active Re-driver
 *           11b - Optically Isolated
 * <18:17> : Cable Current
 *           00b = VBUS is not supported
 *           01b = Reserved
 *           10b = 3A
 *           11b = 5A
 * <16>    : PCIe Supported ? (1b == Yes, 0b == No)
 * <15     : DP Supported ? (1b == Yes, 0b == No)
 * <14>    : TBT Supported ? (1b == Yes, 0b == No)
 * <13>    : Host present ? (1b == Yes, 0b == No)
 * <12:0>  : Reserved
 */
enum usb_mode {
	USB_PD_20,
	USB_PD_32,
	USB_PD_4,
};

enum usb4_cable_current {
	USB4_CABLE_CURRENT_INVALID,
	USB4_CABLE_CURRENT_RESERVED,
	USB4_CABLE_CURRENT_3A,
	USB4_CABLE_CURRENT_5A,
};

enum usb4_cable_type {
	CABLE_TYPE_PASSIVE,
	CABLE_TYPE_ACTIVE_RETIMER,
	CABLE_TYPE_ACTIVE_REDRIVER,
	CABLE_TYPE_ISOLATED,
};

struct enter_usb_payload {
	uint32_t reserved3 : 13;
	uint32_t host_present : 1;
	uint32_t tbt_supported : 1;
	uint32_t dp_supported : 1;
	uint32_t pcie_supported : 1;
	enum usb4_cable_current cable_current : 2;
	enum usb4_cable_type cable_type : 2;
	enum usb_rev30_ss cable_speed : 3;
	uint32_t reserved2 : 1;
	uint32_t usb3_drd_cap : 1;
	uint32_t usb4_drd_cap : 1;
	uint32_t reserved1 : 1;
	enum usb_mode mode : 3;
	uint32_t reserved0 : 1;
};

union enter_usb_data_obj {
	struct enter_usb_payload payload;
	uint32_t raw_value;
};

#endif /* __CROS_EC_USB_PD_VDO_H */
