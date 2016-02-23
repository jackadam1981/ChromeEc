/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Detect what adapter is connected */

#ifndef __CROS_CHARGER_DETECT_H
#define __CROS_CHARGER_DETECT_H

void enable_usb(void);

int get_device_type(void);

#endif /* __CROS_CHARGER_DETECT_H */
