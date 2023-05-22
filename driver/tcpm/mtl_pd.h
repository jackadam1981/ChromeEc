/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Meteorlake compatible USB PD driver header
 * https://cdrdv2.intel.com/v1/dl/getContent/634442
 */

#include "console.h"

/*
 * Table 5-4: DATA STATUS Register Definition (ID-0x5F, RO, Len-5 bytes)
 * -------------------------------------------------------------
 * <39:32> : Reserved
 * <31>    : Reserved
 * <30>    : Reserved
 * <29:28> : USB4/TBT_Cable_Gen
 *           00: 3rd generation USB4/TBT (10.3125 and 20.625 Gb/s)
 *           01: 4th generation USB4/TBT (10.0, 10.3125, 20.0 and 20.625 Gb/s)
 *           10-11: Reserved.
 * <27:25> : USB3.2/USB4/TBT/DP_Cable_Speed_Support
 *           000: USB2 only No USB4/TBT/DP/USB3.2 cable supported
 *           001: USB3.2 USB4/TBT Gen2 (10Gb/s) DP HBR3
 *           010: USB3.2 USB4/TBT Gen2 (10Gb/s) DP UHBR10
 *           011: USB3.2 USB4/TBT Gen3 (20Gb/s) DP UHBR20
 *           100: USB3.2 TBT Gen3 (20Gb/s) USB4 Gen4 (40Gb/s) DP UHBR20
 *           101-111: Reserved
 * <24>    : Power Mismatch (unused)
 *           0 - No USB PD power mismatch
 *           1 - USB PD power mismatch. Not enough power in S0
 * <23>    : 0 = USB4 Not Configured
 *           1 = USB4 Configured
 * <22>    : Active/ Passive
 *           0 = Passive cable
 *           1 = Active cable
 * <21>    : Reserved
 * <20>    : USB4/TBT_Active_Link_Training
 *           0: Active with bidirectional LSRX communication
 *           1: Active with unidirectional LSRX communication
 * <19>    : vPro_Dock_Detected or DP_Overdrive (un used)
 *           0: No vPro Dock. No DP Overdrive detected.
 *           1: vPro Dock or DP Overdrive detected
 * <18>    : Cable_Type
 *           0: Copper/Electrical cable
 *           1: Optical cable
 * <17>    : TBT_Type
 *           0: Type-C to Type-C cable
 *           1: Type-C legacy TBT adapter
 * <16>    : TBT_Connection
 *           0: No TBT connection
 *           1: TBT connection
 * <15>    : HPD_LVL
 *           0: HPD_State low
 *           1: HPD_State High
 * <14>    : HPD_IRQ
 *           0: No IRQ_HPD
 *           1: IRQ_HPD received
 * <13>    : DP IRQ_ACKfmPD (Not used)
 *           0: No IRQ GCRC received
 *           1: IRQ GCRC received
 * <12>    : Debug_Accessory_Mode
 *           0: No Debug accessory mode
 *           1: Debug accessory mode
 * <11:10> : DP_Pin_Assignment
 *           00: Pin assignments E/E’
 *           01: Pin assignments C/C’/D/D’
 *           10: Reserved
 *           11:Reserved
 * <9>     : DP_Source_Sink
 *           0: DP Source connection requested
 *           1: DP Sink connection requested
 * <8>     : DP_Connection
 *           0: No DP connection
 *           1: DP connection
 * <7>     : USB_Data_Role
 *           0: DFP
 *           1: UFP
 * <6>     : USB3.2_Speed
 *           0: USB3.2 is limited to Gen1 (5Gbps) only
 *           1: USB3.2 Gen1/Gen2 supported (5Gbps/10Gbps)
 * <5>     : USB3.2_Connection
 *           0: No USB3.2 connection
 *           1: USB3.2 connection
 * <4>     : USB2_Connection
 *           0: No USB2 connection
 *           1: USB2 connection
 * <3>     : Over_Current_Temp (Not used)
 *           0: An over-current/over-temperature event has not occurred
 *           1: An over-current/over-temperature event has occurred
 * <2>     : Re-Timer_Driver
 *           0: Re-Driver
 *           1: Re-Timer
 * <1>     : Connection Orientation
 *           0: Normal. PD communication on CC1 line
 *           1: Reverse. PD communication on CC2 line.
 * <0>     : Data_Connection_Present
 *           0: No connection present
 *           1: Connection present
 */

#define REG_DATA_STATUS 0x5F
#define DATA_STATUS_REG_LEN 5

union mtl_data_status {
	struct {
		/* Bits 0 to 7 */
		uint8_t data_conn : 1;
		uint8_t conn_ori : 1;
		uint8_t ret_redrv : 1;
		uint8_t oct : 1;
		uint8_t usb2 : 1;
		uint8_t usb3_2 : 1;
		uint8_t usb3_2_speed : 1;
		uint8_t data_role : 1;

		/* Bits 8 to 15 */
		uint8_t dp : 1;
		uint8_t dp_src_snk : 1;
		uint8_t dp_pin : 2;
		uint8_t dbg_acc : 1;
		uint8_t dp_irq : 1;
		uint8_t dp_hpd : 1;
		uint8_t hpd_lvl : 1;

		/* Bits 16 to 23 */
		uint8_t tbt : 1;
		uint8_t tbt_type : 1;
		uint8_t cable_type : 1;
		uint8_t vpro_dock : 1;
		uint8_t usb4_tbt_lt : 1;
		uint8_t res0 : 1;
		uint8_t active_passive : 1;
		uint8_t usb4 : 1;

		/* Bits 24 to 32 */
		uint8_t pow_mis : 1;
		uint8_t cable_speed : 3;
		uint8_t cable_gen : 2;
		uint8_t res1 : 1;
		uint8_t res2 : 1;

		/* Bits 32 to 39 */
		uint8_t res3;
	};
	uint8_t raw_value[DATA_STATUS_REG_LEN];
};

/*
 * Table 5-6: DATA CONTROL Register Definition (ID-0x50, RW, Len-6 bytes)
 * -------------------------------------------------------------
 * <47:16> : Retimer Debug Mode Data
 *           32bits data to be written to Retimer “Debug Mode” register
 *           (Reg Num – 0x07)
 * <15>    : Reserved
 * <14>    : Reserved
 * <13>    : HPD_IRQ_ACK
 *           0: No HPD_IRQ_ACK
 *           1: HPD_IRQ_ACK
 * <12>    : Write_to_Retimer
 *           0: No action. Ignore Bytes 3-6
 *           1: Write Bytes 3-6 to Retimer
 * <11:8>  : Reserved
 * <7:3>   : Reserved
 * <2>     : I2C_INT_ACK
 *           0: Do Nothing
 *           1: SoC acknowledge for the interrupt
 * <1:0>   : Reserved
 */

#define REG_DATA_CONTROL 0x50
#define DATA_CONTROL_REG_LEN 6

union mtl_data_control {
	struct {
		/* Bits 0 to 7 */
		uint8_t res0 : 2;
		uint8_t i2c_int_ack : 1;
		uint8_t res1 : 5;

		/* Bits 7 to 15 */
		uint8_t res2 : 4;
		uint8_t wr_ret : 1;
		uint8_t hpd_irq_ack : 1;
		uint8_t res3 : 2;

		/* Bits 16 to 47 */
		uint32_t ret_dbg_mode;
	};
	uint8_t raw_value[DATA_CONTROL_REG_LEN];
};
