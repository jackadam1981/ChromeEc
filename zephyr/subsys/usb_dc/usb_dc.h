/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB DC Shimming Definitions.
 */

#ifndef __USB_DC_H
#define __USB_DC_H

#include "common.h"

bool check_usb_is_suspended(void);
void request_usb_wake(void);

#endif
