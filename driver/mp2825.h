/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PMIC_MP2825_H
#define __CROS_EC_PMIC_MP2825_H

#define MP2825_PAGE 0x00
#define MP2825_OPERATION 0x01
#define MP2825_CLEAR_CRC_FAULT 0x03
#define MP2825_STORE_USER_ALL 0x15
#define MP2825_RESTORE_USER_ALL 0x16
#define MP2825_MFR_VOUT_TRIM 0x22
#define MP2825_MFR_PHASE_NUM 0x29
#define MP2825_MFR_IMON_SNS_OFFS 0x2c
#define MP2825_IOUT_CAL_GAIN_SET 0x38
#define MP2825_MFR_TRANS_FAST 0x3d
#define MP2825_MFR_ALT_SET 0x3f
#define MP2825_MFR_CONFIG2 0x48
#define MP2825_MFR_SLOPE_SR_DCM 0x4e
#define MP2825_MFR_ICC_MAX_SET 0x53
#define MP2825_MFR_OCP_OVP_DAC_LIMIT 0x60
#define MP2825_MFR_DEVICE_ID 0x62
#define MP2825_MFR_I2C_PASSWORD 0x82
#define MP2825_PRODUCT_DATA_CODE 0x93
#define MP2825_LOT_CODE_VR 0x94
#define MP2825_MFR_READ_CRC 0xae
#define MP2825_MFR_PSI_TRIM4 0xb0
#define MP2825_MFR_PSI_TRIM1 0xb1
#define MP2825_MFR_PSI_TRIM3 0xb3
#define MP2825_MFR_CRC_FAULT 0xbe
#define MP2825_MFR_PASSWORD_UNLOCK 0xd0
#define MP2825_MFR_SLOPE_CNT_2P 0xd4
#define MP2825_MFR_SLOPE_CNT_5P 0xe0
#define MP2825_MFR_IMON_SVID1 0xe8
#define MP2825_MFR_IMON_SVID2 0xe9
#define MP2825_MFR_IMON_SVID3 0xea
#define MP2825_MFR_IMON_SVID4 0xeb
#define MP2825_MFR_IMON_SVID5 0xef
#define MP2825_MFR_IMON_SVID6 0xf0

struct mp2825_reg_val {
	uint8_t reg;
	uint16_t val;
};

int mp2825_tune(const struct mp2825_reg_val *page0, int count0,
		const struct mp2825_reg_val *page1, int count1,
		const struct mp2825_reg_val *page2, int count2);

#endif /* __CROS_EC_PMIC_MP2825_H */
