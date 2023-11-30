/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __USBD_INIT_H
#define __USBD_INIT_H

#include <stdbool.h>

bool check_usb_is_suspended(void);
bool request_usb_wake(void);

#endif
