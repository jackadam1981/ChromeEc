/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_DPM_H
#define __CROS_EC_USB_DPM_H

void dpm_init(int port);
void dpm_set_mode_entry_done(int port);
void dpm_send_svdm(int port);
void dpm_attempt_mode_entry(int port);

#endif  /* __CROS_EC_USB_DPM_H */
