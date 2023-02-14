/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_mux.h"
#include "usbc/usb_muxes.h"

#include <zephyr/devicetree.h>
#include <zephyr/sys/util_macro.h>

const struct kb8010_control kb8010_controls[] = {
	USB_MUX_KB8010_CONTROLS_ARRAY
};
