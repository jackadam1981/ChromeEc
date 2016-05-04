/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power Delivery Display Port alternate mode helpers */

#ifndef __CROS_EC_USB_PD_ALTMODE_DP_H
#define __CROS_EC_USB_PD_ALTMODE_DP_H

int dp_set_hpd(int port, int enable);
int dp_get_hpd(int port);

int dp_set_irq(int port, int enable);
#endif /* __CROS_EC_USB_PD_ALTMODE_DP_H */
