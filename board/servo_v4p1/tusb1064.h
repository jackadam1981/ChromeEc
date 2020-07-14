/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TUSB1064_H
#define __CROS_EC_TUSB1064_H

#include <stdint.h>

#define TUSB1064_ADDR_FLAGS		0x12

#define TUSB1064_REG_GENERAL		0x0a
#define REG_GENERAL_CTLSEL_DISABLE      0x00
#define REG_GENERAL_CTLSEL_USB3_1       0x01
#define REG_GENERAL_CTLSEL_4_DP_LANES   0x02
#define REG_GENERAL_CTLSEL_2_DP_AND_USB3_1 0x03
#define REG_GENERAL_FLIPSEL		BIT(2)
#define REG_GENERAL_DP_ENABLE_CTRL	BIT(3)
#define REG_GENERAL_EQ_OVERRIDE		BIT(4)

#define TUSB1064_REG_10			0x10
#define REG_10_DP3EQ_SEL(n)		(n & 0xf)
#define REG_10_DP1EQ_SEL(n)		((n & 0xf) << 4)

#define TUSB1064_REG_11			0x11
#define REG_11_DP2EQ_SEL(n)		(n & 0x0f)
#define REG_11_DP0EQ_SEL(n)		((n & 0xf) << 4)

#define TUSB1064_REG_12			0x12
#define REG_12_LANE_COUNT_SET(n)	(n & 0x1f)
#define REG_12_SET_POWER_STATE(n)	((n >> 5) & 3)

#define TUSB1064_REG_13			0x13
#define REG_13_DP0_DISABLE		BIT(0)
#define REG_13_DP1_DISABLE		BIT(1)
#define REG_13_DP2_DISABLE		BIT(2)
#define REG_13_DP3_DISABLE		BIT(3)
#define REG_13_AUX_SBU_OVR(n)		((n & 3) << 4)
#define AUX_TO_SBU_CONN			0
#define AUXN_TO_SBU1_AUXP_TO_SBU2	1
#define AUXN_TO_SBU2_AUXP_TO_SBU1	2
#define AUX_TO_SBU_OPEN			3
#define REG_13_AUX_SNOOP_DISABLE	BIT(7)

/*
 * Initialize the TUSB1064
 *
 * @param port	The I2C port of TUSB1064
 * @return EC_SUCCESS or EC_ERROR_*
 */
int init_tusb1064(int port);

/*
 * Write a byte to the TUSB1064
 *
 * @param port	The I2C port of TUSB1064.
 * @param reg	Register to write byte to.
 * @param val	Value to write to TUSB1064.
 *
 * @return EC_SUCCESS, or EC_ERROR_* on error.
 */
int tusb1064_write_byte(int port, uint8_t reg, uint8_t val);

/*
 * Read a byte from TUSB1064
 *
 * @param port	The I2C port of TUSB1064.
 * @param reg	Register to read byte from.
 *
 * @return	byte value, or -1 on error.
 */
int tusb1064_read_byte(int port, uint8_t reg);

#endif /* __CROS_EC_TUSB1064_H */
