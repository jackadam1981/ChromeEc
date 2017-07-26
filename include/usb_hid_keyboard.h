/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB HID definitions.
 */

#ifndef __CROS_EC_USB_HID_KEYBOARD_H
#define __CROS_EC_USB_HID_KEYBOARD_H

/**
 * Call board-specific keyboard backlight setting function.
 */
void board_set_backlight(int brightness);

#endif
