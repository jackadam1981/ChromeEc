/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7483: Active redriver with linear equilzation
 */

#ifndef __CROS_EC_USB_RETIMER_ANX7452_H
#define __CROS_EC_USB_RETIMER_ANX7452_H

#define ANX7452_TOP_CTRL_REG 0xF8
#define ANX7452_TOP_CTRL_USB3_EN BIT(0)
#define ANX7452_TOP_CTRL_DP_EN BIT(1)
#define ANX7452_TOP_CTRL_TBT_EN BIT(2)
#define ANX7452_TOP_CTRL_USB4_EN BIT(3)
#define ANX7452_TOP_CTRL_FLIP_EN BIT(4)
#define ANX7452_TOP_CTRL_SWAP_EN BIT(5)
#define ANX7452_TOP_CTRL_REG_EN BIT(7)

#define ANX7452_CTLTOP_FLIP_REG 0x04
#define ANX7452_CTLTOP_USB3_REG 0x04
#define ANX7452_CTRL_FLIP_EN BIT(1)
#define ANX7452_CTRL_USB3_EN BIT(5)

#define ANX7452_CTLTOP_DP_REG 0x05
#define ANX7452_CTRL_DP_EN BIT(0)

#define ANX7452_CTLTOP_TBT_REG 0x06
#define ANX7452_CTLTOP_USB4_REG 0x06
#define ANX7452_CTRL_TBT_EN BIT(0)
#define ANX7452_CTRL_USB4_EN BIT(7)

#endif /* __CROS_EC_USB_RETIMER_ANX7483_H */
