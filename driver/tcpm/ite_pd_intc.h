/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ITE PD INTC control module */

#ifndef __CROS_EC_ITE_PD_INTC_H
#define __CROS_EC_ITE_PD_INTC_H

#ifdef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
extern void chip_pd_irq(enum usbpd_port port);
#endif

#endif /* __CROS_EC_ITE_PD_INTC_H */
