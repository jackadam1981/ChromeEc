/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7483: Active redriver with linear equilzation
 */

#ifndef __CROS_EC_USB_RETIMER_ANX7452_H
#define __CROS_EC_USB_RETIMER_ANX7452_H

/*
 * TOP Control register
 *
 * 7   EN (0: Config info from pins, 1: Config info from registers)
 * 6   Reserved
 * 5   SWAP (0: host side, 1: device side)
 * 4   FLIP info (Read only use)
 * 3   USB4 info (Read only use)
 * 2   TBT info (Read only use)
 * 1   DP info (Read only use)
 * 0   USB3 info (Read only use)
 */
#define ANX7452_TOP_CTRL_REG 0xF8
#define ANX7452_TOP_CTRL_REG_EN BIT(7)
#define ANX7452_TOP_CTRL_SWAP_EN BIT(5)
#define ANX7452_TOP_CTRL_FLIP_EN BIT(4)
#define ANX7452_TOP_CTRL_USB4_EN BIT(3)
#define ANX7452_TOP_CTRL_TBT_EN BIT(2)
#define ANX7452_TOP_CTRL_DP_EN BIT(1)
#define ANX7452_TOP_CTRL_USB3_EN BIT(0)

/*
 * CTLTOP I2C register address on TOP
 */
#define ANX7452_TOP_CTLTOP_REG_ADDR_REG 0x91

/*
 * CTLTOP FLIP and USB3 register
 *
 * 5   USB3 info (To set Bit 0 of TOP Control register indirectly)
 * 1   FLIP info (To set BIT 4 of TOP Control register indirectly)
 */
#define ANX7452_CTLTOP_USB3_REG 0x04
#define ANX7452_CTRL_USB3_EN BIT(5)

#define ANX7452_CTLTOP_FLIP_REG 0x04
#define ANX7452_CTRL_FLIP_EN BIT(1)

/*
 * CTLTOP DP register
 *
 * 0   DP info (To set Bit 1 of TOP Control register indirectly)
 */
#define ANX7452_CTLTOP_DP_REG 0x05
#define ANX7452_CTRL_DP_EN BIT(0)

/*
 * CTLTOP TBT and USB4 register
 *
 * 7   USB4 info (To set Bit 3 of TOP Control register indirectly)
 * 0   TBT info (To set BIT 2 of TOP Control register indirectly)
 */
#define ANX7452_CTLTOP_USB4_REG 0x06
#define ANX7452_CTRL_USB4_EN BIT(7)

#define ANX7452_CTLTOP_TBT_REG 0x06
#define ANX7452_CTRL_TBT_EN BIT(0)

/* CTLTOP I2C register address will be stored in this variable and used by the
 * driver code. It gets assigned with actual value in the init function */
int ctltop_reg_i2c_addr = 0;

#endif /* __CROS_EC_USB_RETIMER_ANX7483_H */
