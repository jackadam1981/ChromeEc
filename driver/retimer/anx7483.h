/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7483: Active redriver with linear equilzation
 */

#ifndef __CROS_EC_USB_RETIMER_ANX7483_H
#define __CROS_EC_USB_RETIMER_ANX7483_H

#define ANX7483_ANALOG_STATUS_CTRL_REG	0x07
#define ANX7483_CTRL_REG_BYPASS_EN	BIT(5)
#define ANX7483_CTRL_REG_EN		BIT(4)
#define ANX7483_CTRL_FLIP_EN		BIT(2)
#define ANX7483_CTRL_DP_EN		BIT(1)
#define ANX7483_CTRL_USB_EN		BIT(0)

#define ANX7483_ENABLE_EQ_FLAT_SWING_REG	0x15
#define ANX7483_ENABLE_EQ_FLAT_SWING_EN		BIT(0)

enum anx7483_eq_setting {
	ANX7483_EQ_SETTING_3_9DB = 0,
	ANX7483_EQ_SETTING_4_7DB = 1,
	ANX7483_EQ_SETTING_5_5DB = 2,
	ANX7483_EQ_SETTING_6_1DB = 3,
	ANX7483_EQ_SETTING_6_8DB = 4,
	ANX7483_EQ_SETTING_7_3DB = 5,
	ANX7483_EQ_SETTING_7_8DB = 6,
	ANX7483_EQ_SETTING_8_1DB = 7,
	ANX7483_EQ_SETTING_8_4DB = 8,
	ANX7483_EQ_SETTING_8_7DB = 9,
	ANX7483_EQ_SETTING_9_2DB = 10,
	ANX7483_EQ_SETTING_9_7DB = 11,
	ANX7483_EQ_SETTING_10_3DB = 12,
	ANX7483_EQ_SETTING_11_1DB = 13,
	ANX7483_EQ_SETTING_11_8DB = 14,
	ANX7483_EQ_SETTING_12_5DB = 15,
};

/* EQ Settings Registers */
#define ANX7483_UTX1_PORT_CFG0_REG	0x52
#define ANX7483_UTX2_PORT_CFG0_REG	0x16
#define ANX7483_URX1_PORT_CFG0_REG	0x3E
#define ANX7483_URX2_PORT_CFG0_REG	0x2A
#define ANX7483_DRX1_PORT_CFG0_REG	0x5C
#define ANX7483_DRX2_PORT_CFG0_REG	0x20

/* EQ values are bits 7:4 in each register */
#define ANX7483_EQ_SHIFT		4
/* Reserved bits set - check with vendor */
#define ANX7483_CFG0_DEF		0x53

enum anx7483_flat_gain {
	ANX7483_FLAT_GAIN_NEG_1_5DB = 0,
	ANX7483_FLAT_GAIN_NEG_0_5DB = 1,
	ANX7483_FLAT_GAIN_0_3DB = 2,
	ANX7483_FLAT_GAIN_1_2DB = 3,
};

/* Flat Gain Settings Registers */
#define ANX7483_UTX1_PORT_CFG2_REG	0x54
#define ANX7483_UTX2_PORT_CFG2_REG	0x18
#define ANX7483_URX1_PORT_CFG2_REG	0x40
#define ANX7483_URX2_PORT_CFG2_REG	0x2C
#define ANX7483_DRX1_PORT_CFG2_REG	0x5E
#define ANX7483_DRX2_PORT_CFG2_REG	0x22

/* Flat Gain values are bits 5:4 in each register */
#define ANX7483_FLAT_GAIN_SHIFT		4
/* Reserved bits set - check with vendor */
#define ANX7483_CFG2_DEF		0xEE

enum anx7483_swing_setting {
	ANX7483_SWING_SETTING_800MV = 0,
	ANX7483_SWING_SETTING_1000MV = 1,
	ANX7483_SWING_SETTING_1200MV = 2,
	ANX7483_SWING_SETTING_1300MV = 3,
};

/* Swing and 60K Input Termination Registers */
#define ANX7483_UTX1_PORT_CFG4_REG	0x56
#define ANX7483_UTX2_PORT_CFG4_REG	0x1A
#define ANX7483_URX1_PORT_CFG4_REG	0x42
#define ANX7483_URX2_PORT_CFG4_REG	0x2E
#define ANX7483_DRX1_PORT_CFG4_REG	0x60
#define ANX7483_DRX2_PORT_CFG4_REG	0x24
#define ANX7483_DTX1_PORT_CFG4_REG	0x4C
#define ANX7483_DTX2_PORT_CFG4_REG	0x38

/* All above registers have 60K Input termination in bit 4 */
#define ANX7483_RTERM60K_EN		BIT(4)
/* All above registers except DTX1/2 have swing in bits 1:0 */
#define ANX7483_SWING_SHIFT		0
/* Reserved bits set - check with vendor. Names based on bit 4 flips */
#define ANX7483_CFG4_TERM_DISABLE	0x63
#define ANX7483_CFG4_TERM_ENABLE	0x73

/* Termination Resistance Registers */
#define ANX7483_UTX1_PORT_CFG3_REG	0x55
#define ANX7483_UTX2_PORT_CFG3_REG	0x19
#define ANX7483_URX1_PORT_CFG3_REG	0x41
#define ANX7483_URX2_PORT_CFG3_REG	0x2D
#define ANX7483_DTX1_PORT_CFG3_REG	0x4B
#define ANX7483_DTX2_PORT_CFG3_REG	0x37
#define ANX7483_DRX1_PORT_CFG3_REG	0x5F
#define ANX7483_DRX2_PORT_CFG3_REG	0x23

/* Reserved bits set - check with vendor to find meaningful names */
#define ANX7483_CFG3_3A			0x3A
#define ANX7483_CFG3_7A			0x7A
#define ANX7483_CFG3_7E			0x7E

#define ANX7483_AUX_SNOOPING_CTRL_REG	0x13
/* Reserved bits set - check with vendor */
#define ANX7483_AUX_SNOOPING_DEF	0x13

/* Middle Frequency Compensation */
#define ANX7483_UTX1_PORT_CFG1_REG	0x53
#define ANX7483_UTX2_PORT_CFG1_REG	0x17
#define ANX7483_URX1_PORT_CFG1_REG	0x3F
#define ANX7483_URX2_PORT_CFG1_REG	0x2B
#define ANX7483_DRX1_PORT_CFG1_REG	0x5D
#define ANX7483_DRX2_PORT_CFG1_REG	0x21

/*
 * Default CFG1 setting:
 * - 7:6 CTLE current bias max
 * - 5:3 Middle frequency resistance of 0x5
 * - 2:0 Middle frequency capacitance of 0x6
 */
#define ANX7483_CFG1_DEF		0xEE

#endif /* __CROS_EC_USB_RETIMER_ANX7483_H */
