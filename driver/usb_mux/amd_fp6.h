/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMD FP5 USB/DP Mux.
 */

#ifndef __CROS_EC_USB_MUX_AMD_FP6_H
#define __CROS_EC_USB_MUX_AMD_FP6_H

#define AMD_FP6_C0_MUX_I2C_ADDR	0x5C
#define AMD_FP6_C4_MUX_I2C_ADDR	0x52

#define AMD_FP6_MUX_MODE_SAFE	0x0
#define AMD_FP6_MUX_MODE_USB	0x1
#define AMD_FP6_MUX_MODE_DP	0x2
#define AMD_FP6_MUX_MODE_DOCK	0x3
#define AMD_FP6_MUX_MODE_OFFSET	8
#define AMD_FP6_MUX_MODE_MASK	GENMASK(9, 8)

#define AMD_FP6_MUX_ORIENTATION_OFFSET	12

#define AMD_FP6_MUX_LOW_POWER_OFFSET	13

#define AMD_FP6_MUX_STATUS_READY_OFFSET	5

#endif /* __CROS_EC_USB_MUX_AMD_FP6_H */
