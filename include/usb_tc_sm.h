/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Type-C module */

#ifndef __CROS_EC_USB_TC_H
#define __CROS_EC_USB_TC_H

void tc_hard_reset(int port);
int tc_get_data_role(int port);
int tc_get_power_role(int port);
void tc_disable_pd(int port);
void tc_set_vconn(int port, int enable);
#endif

