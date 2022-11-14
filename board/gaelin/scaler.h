/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCALER_H
#define __CROS_EC_SCALER_H

void osd_int_interrupt(enum gpio_signal signal);
void disp_mode_interrupt(enum gpio_signal signal);
void hdmi0_cable_det_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_SCALER_H */
