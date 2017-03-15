/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_GENVIF_H
#define __CROS_EC_GENVIF_H

/* From board usb_pd_policy.c */
extern const uint32_t pd_src_pdo[];
extern const int pd_src_pdo_cnt;

#ifdef CONFIG_USB_PD_DUAL_ROLE
/* From board usb_pd_policy.c */
extern const uint32_t pd_snk_pdo[];
extern const int pd_snk_pdo_cnt;
#endif

#endif /* __CROS_EC_GENVIF_H */
