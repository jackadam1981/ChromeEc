/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_DPM_H
#define __CROS_EC_USB_DPM_H

#include "include/hooks.h"

void dpm_send_svdm(int port);

#endif  /* __CROS_EC_USB_DPM_H */
