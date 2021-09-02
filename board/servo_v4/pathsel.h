/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PATHSEL_H
#define __CROS_EC_PATHSEL_H

/**
 * Routes the DUT to the HOST. Used for fastboot
 */
void dut_to_host(void);

/**
 * Routes the Micro Servo to the Host
 */
void uservo_to_host(void);

#endif /* __CROS_EC_PATHSEL_H */
