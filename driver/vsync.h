/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * VSYNC driver
 */

#ifndef __CROS_EC_VSYNC_H
#define __CROS_EC_VSYNC_H

// #define BH1730_GET_DATA(_s)    ((struct bh1730_drv_data_t *)(_s)->drv_data)
//
// struct bh1730_drv_data_t {
// 	int rate;
// 	int last_value;
// };

extern const struct accelgyro_drv vsync_drv;

void vsync_interrupt(enum gpio_signal signal);

#endif	/* __CROS_EC_VSYNC_H */

