/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCALER_H
#define __CROS_EC_SCALER_H

#define SCALER_NAV_KEY_REG 0x0381
#define SCALER_VOL_UP 0x01
#define SCALER_VOL_DOWN 0x02
#define SCALER_BRIGHTNESS_UP 0x04
#define SCALER_BRIGHTNESS_DOWN 0x08

void disp_mode_interrupt(enum gpio_signal signal);
void hdmi_5v_in_interrupt(enum gpio_signal signal);
void hdmi0_cable_det_interrupt(enum gpio_signal signal);
void osd_int_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_SCALER_H */
