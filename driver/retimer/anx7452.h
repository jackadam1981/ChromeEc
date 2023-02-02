/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7483: Active redriver with linear equilzation
 */

#ifndef __CROS_EC_USB_RETIMER_ANX7452_H
#define __CROS_EC_USB_RETIMER_ANX7452_H

#define ANX7452_TOP 0xF8
#define ANX7452_CTRL_TOP_1 0x04
#define ANX7452_CTRL_TOP_2 0x05
#define ANX7452_CTRL_TOP_3 0x06
#define ANX7452_TOP_SWAP_EN BIT(5)
#define ANX7452_TOP_FLIP_EN BIT(4)
#define ANX7452_TOP_DP_EN BIT(1)
#define ANX7452_TOP_USB4_EN BIT(3)
#define ANX7452_TOP_USB3_EN BIT(0)
#define ANX7452_TOP_TBT_EN BIT(2)
#define ANX7452_CTRL_TOP_1_FLIP_EN BIT(1)
#define ANX7452_CTRL_TOP_2_DP_EN BIT(0)
#define ANX7452_CTRL_TOP_3_USB4_EN BIT(7)
#define ANX7452_CTRL_TOP_1_USB3_EN BIT(5)
#define ANX7452_CTRL_TOP_3_TBT_EN BIT(0)

#endif /* __CROS_EC_USB_RETIMER_ANX7483_H */
