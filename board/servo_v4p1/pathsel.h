/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PATHSEL_H
#define __CROS_EC_PATHSEL_H

/**
 * Both USB3_TypeA0 and USB3_TypeA1 are routed to the DUT by default.
 */
void init_pathsel(void);

/**
 * Routes USB3_TypeA0 port to DUT
 */
void usb3_a0_to_dut(void);

/**
 * Routes USB3_TypeA1 port to DUT
 */
void usb3_a1_to_dut(void);

/**
 * Routes USB3_TypeA0 port to HOST
 */
void usb3_a0_to_host(void);

/**
 * Routes USB3_TypeA1 port to HOST
 */
void usb3_a1_to_host(void);

/**
 * Routes the DUT to the HOST. Used for fastboot
 */
void dut_to_host(void);

/**
 * Routes the Micro Servo to the Host
 */
void uservo_to_host(void);

/**
 * Controls load switches for 5V to A0 general USB type A.
 *
 * @param en	0 - Disable power, 1 - Enable power
 * @return EC_SUCCESS or EC_xxx on error
 */
int usb3_a0_pwr_en(int en);

/**
 * Controls load switches for 5V to A1 general USB type A port.
 *
 * @param en	0 - Disable power, 1 - Enable power
 * @return EC_SUCCESS or EC_xxx on error
 */
int usb3_a1_pwr_en(int en);

/**
 * Controls load switches for 5V to uservo USB type A port.
 *
 * @param en	0 - Disable power, 1 - Enable power
 * @return EC_SUCCESS or EC_xxx on error
 */
int uservo_pwr_en(int en);

#endif /* __CROS_EC_PATHSEL_H */
