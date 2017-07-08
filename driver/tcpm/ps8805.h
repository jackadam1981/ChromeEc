/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Parade Tech Type-C controller PS8805 register mapping. */

#ifndef __CROS_EC_USB_PD_TCPM_PS8805_H
#define __CROS_EC_USB_PD_TCPM_PS8005_H

/* Vendor defined registers */
#define PS8805_VENDOR_ID  0x1DA0
#define PS8805_PRODUCT_ID 0x8805

#define PS8805_REG_FW_REVISION_ID 0x82
#define PS8805_REG_MUX_IN_HPD_ASSERTION 0xD0
#define PS8805_REG_MUX_IN_HPD_ASSERTION_IN_HPD  (1 << 0)
#define PS8805_REG_MUX_IN_HPD_ASSERTION_HPD_IRQ (1 << 1)

#endif /* __CROS_EC_USB_PD_TCPM_PS8805_H */
