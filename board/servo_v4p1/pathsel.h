/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PATHSEL_H
#define __CROS_EC_PATHSEL_H

void init_pathsel(void);
void usb3_a0_to_dut(void);
void usb3_a1_to_dut(void);
void usb3_a0_to_host(void);
void usb3_a1_to_host(void);

#endif /* __CROS_EC_PATHSEL_H */
