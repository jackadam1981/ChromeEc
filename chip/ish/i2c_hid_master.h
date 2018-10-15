/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* HIS I2C Master Implementation */
/* Only support single slave device with pre-defined parameters */
#ifndef __CROS_EC_I2C_HID_MASTER_H
#define __CROS_EC_I2C_HID_MASTER_H

#include <stdint.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

/* Currently only support ELAN TSC as HID slave device */
#define HIDI2C_SLAVEDEV_TSC_ELAN_B50	1

#if defined(HIDI2C_SLAVEDEV_TSC_ELAN_B50)
/* Basic Finger */
#define MAX_FINGER_CNT			5
/* HID */
#define HIDDESC_LNG_HIDDESC             HID_DESC_LENGTH
#define HIDDESC_LNG_MAXRPTDESC          0x02FD
#define HIDDESC_LNG_MAXINPUT            0x0038
#define HIDDESC_LNG_MAXOUTPUT           0x0000
/* Test Mode */

struct __attribute__((packed)) frame_hdr {
	uint8_t	length_lsb;
	uint8_t	length_msb;
	uint8_t	rid;
	uint8_t	index;
};

struct __attribute__((packed)) data_hdr {
	uint8_t	length_lsb;
	uint8_t	length_msb;
	uint8_t	rid;
};

union hid_report {
	uint8_t raw[HIDDESC_LNG_MAXINPUT];
	struct {
		struct data_hdr hdr;
		union {
			struct __attribute__((packed)) {
				uint8_t	button		:1;
				uint8_t	rsvd0		:3;
				uint8_t	contact_num	:4;
				uint8_t	scantime_lsb;
				uint8_t	scantime_msb;
				struct __attribute__((packed)) {
					uint8_t	confidence	:1;
					uint8_t	tip_switch	:1;
					uint8_t	rsvd0		:2;
					uint8_t	contact_id	:4;
					uint8_t	x_lsb;
					uint8_t	x_msb;
					uint8_t	y_lsb;
					uint8_t	y_msb;
					uint8_t width_lsb;
					uint8_t	width_msb;
					uint8_t	height_lsb;
					uint8_t	height_msb;
					uint8_t pressure;
				} fingers[MAX_FINGER_CNT];
			} ptp_mode_report;
			struct __attribute__((packed)) {
				uint8_t button_left	:1;
				uint8_t button_right	:1;
				uint8_t rsvd0		:6;
				uint8_t x;
				uint8_t	y;
				uint8_t rsvd1[5];
			} mouse_mode_report;
		};
	};
};

enum test_mode_idx {
	SELF_DV		= 0,
	SELF_BASE	= 1,
	SELF_RAW	= 2,
	MUTUAL_DV	= 3,
	MUTUAL_BASE	= 4,
	MUTUAL_RAW	= 5,
	OPENSHORT	= 6,
	TESTMODMAX,
};
#endif

#define INTP_NOACT	0
#define INTP_ACT	1

/* ====================== General HID Master APIs ======================= */

/**
 * Get HID descriptor from slave device. Slave address and the address of
 * HID descriptor are fixed in source code. The content of HID descriptor
 * will be stored and managed inside.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_get_hid_desc(void);

int32_t i2c_hid_copy_hid_desc(uint8_t *buf);

/**
 * Get report descriptor from slave device. Slave address, the address of
 * report descriptor and the length are fixed in source code. The content
 * of RPT descriptor will be stored and managed inside.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_get_rpt_desc(void);
int32_t i2c_hid_copy_rpt_desc(uint8_t *buf);


/**
 * Get input report from slave device. Slave address, the address of input
 * report and the length are fixed in source code. 
 *
 * @param intpAct	If current action is triggered by interrupt.
 * @param pData		Data buffer for receiving input report.
 * @param maxLng	The maximum size of pData.
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_get_input(uint32_t interrupt, uint8_t *data, uint32_t max_lng);

/**
 * Send HID reset command to slave device. Slave address, the address of
 * command register are fixed in source code. 
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_reset(void);

/**
 * Send HID sleep command to slave device. Slave address, the address of
 * command register are fixed in source code.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_sleep(void);

/**
 * Send HID wakeup command to slave device. Slave address, the address of
 * command register are fixed in source code.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_wakeup(void);

#if defined(HIDI2C_SLAVEDEV_TSC_ELAN_B50)
/* =================== B50 ELAN TSC Specific HID APIs ==================== */

/**
 * Send input mode command with PTP mode data. Slave address, the address
 * of command register are fixed in source code.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_set_ptp(void);

/**
 * Send input mode command with Mouse mode data. Slave address, the address
 * of command register are fixed in source code.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_set_mouse(void); 

/**
 * Get heatmap from TSC. Send get report command with report type 11b(Feature)
 * and report ID 0x09(ELAN heatmap). Slave address, the address of command
 * register are fixed in source code.
 *
 * @param pRcvBuf	Buffer for receiving data from device.
 * @param lng		Total receive length.
 *
 * @return non-zero if error occurred.
 *
 * @note Recive buffer format:
 * 	----------------------------------------------------------
 * 	|Byte\Bit|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * 	----------------------------------------------------------
 * 	|    0   |                 Data Lng LSB                  |
 * 	----------------------------------------------------------
 * 	|    1   |                 Data Lng MSB                  |
 * 	----------------------------------------------------------
 * 	|    2   |                Report ID(0x09)                |
 * 	----------------------------------------------------------
 * 	|    3   |                 Frame Index                   |
 * 	----------------------------------------------------------
 * 	|  4 ~ N |   Frame Data(Size 26*15 + 15 + 26 for B50)    |
 * 	----------------------------------------------------------
 *
 * @note Total receive length:
 *	 Total Length = 2 bytes "Data Lng" + 1 byte RID + 1 byte fidx
 *	 		+ n bytes image = n + 4.
 *
 */
int32_t i2c_hid_get_heatmap(uint8_t *data, uint32_t lng);


/**
 * Get firmware UID from TSC. FW UID is a combination of FWID and
 * FWVER which should be unique from different release of FW.
 *
 * @param pFwUid       Buffer for returning FWUID.
 *
 * @return non-zero if error occurred.
 *
 * @note FWUID format:
 *  -------------------------------------------------------------
 *  |  Byte   |      0     |     1      |     2     |     3     |
 *  -------------------------------------------------------------
 *  | Purpose | FW VER LSB | FW VER MSB | FW ID LSB | FW ID MSB |
 *  -------------------------------------------------------------
 *
 * @note Please refer to ELAN production test programming guide.
 */
int32_t i2c_hid_get_fwuid(uint32_t *fwuid);

/**
 * Get flash check sum from TSC. Host should check with the CheckSum
 * From TSC vendor.
 *
 * @param pChkSum       Buffer for returning check sum.
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_get_chksum(uint16_t *chksum);

/**
 * Start specific test mode. In test mode the auto-idle will be
 * disabled. The sequence of test mode command should be:
 *   Start => SetStatChk => GetStatChk => GetData => Exit
 *
 * @param TestModIdx	For different testing items. Please refer
 * 			to enum define.
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_testmode_start(enum test_mode_idx testidx);

/**
 * Get the testing data. For different testing item the testing
 * data will also be different. Please refer to ELAN B50
 * programming guide.
 *
 * @param pBuf		Data buffer from caller.
 * @param lng		Data length to read from device.
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_testmode_get(uint8_t *data, uint32_t lng);

/**
 * Exit the test mode.
 *
 * @param None.
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_testmode_stop(void);

/**
 * Disable the auto-idle mode in ELAN TSC.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_disable_autoidle(void);


/**
 * Enable the auto-idle mode in ELAN TSC.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int32_t i2c_hid_enable_autoidle(void);
#endif //HIDI2C_SLAVEDEV_TSC_ELAN_B50

#endif /* __CROS_EC_I2C_HID_MASTER_H */
