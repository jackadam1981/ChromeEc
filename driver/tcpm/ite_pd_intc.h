/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ITE PD INTC control module */

#ifndef __CROS_EC_ITE_PD_INTC_H
#define __CROS_EC_ITE_PD_INTC_H

/**
 * ITE embedded PD interrupt routine
 *
 * NOTE: Enable ITE embedded PD that it requires CONFIG_USB_PD_TCPM_ITE_ON_CHIP
 *
 * @param port Type-C port number
 *
 * @return none
 */
void chip_pd_irq(enum usbpd_port port);

/**
 * ITE timer interrupt routine for software auto toggle
 *
 * NOTE: In the future, we will have auto toggle register as TCPC spec defined,
 *       then we may needn't this routine
 *
 * @param port Type-C port number
 *
 * @return none
 */
void auto_toggle_timer_interrupt(enum usbpd_port port);

#endif /* __CROS_EC_ITE_PD_INTC_H */
