/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternate Mode Upstream Facing Port (UFP) USB-PD module.
 */
#include "usb_pd.h"
#include "usb_tbt_alt_mode.h"

/* Return port partner's enter mode message */
__overridable union tbt_dev_mode_enter_cmd pd_ufp_get_enter_mode(int port)
{
	union tbt_dev_mode_enter_cmd ufp_enter_mode = {.raw_value = 0};
	return ufp_enter_mode;
}

/* Clear alternate mode flag */
__overridable void ufp_clear_alt_mode(int port)
{
}

/* Set retimer into alternate mode */
__overridable void ufp_mux_set_alt_mode(int port)
{
}
