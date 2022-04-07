/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <devicetree.h>
#include <sys/util_macro.h>
#include "usb_mux.h"
#include "usbc/it5205_usb_mux.h"
#include "usbc/tusb1064_usb_mux.h"
#include "usbc/usb_muxes.h"
#include "usbc/virtual_usb_mux.h"

#if DT_HAS_COMPAT_STATUS_OKAY(IT5205_USB_MUX_COMPAT) ||			\
	DT_HAS_COMPAT_STATUS_OKAY(TUSB1064_USB_MUX_COMPAT) ||		\
	DT_HAS_COMPAT_STATUS_OKAY(VIRTUAL_USB_MUX_COMPAT)

/**
 * Define root of each USB muxes chain e.g.
 * [0] = {
 *         .usb_port = 0,
 *         .next_mux = &USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_it5205_mux_0,
 *         .board_init = &board_init,
 *         .board_set = NULL,
 *         .flags = 0,
 *         .driver = &virtual_usb_mux_driver,
 *         .hpd_update = &virtual_hpd_update,
 * },
 * [1] = { ... },
 */
MAYBE_CONST struct usb_mux usb_muxes[] = {
	USB_MUX_FOREACH_USBC_PORT(USB_MUX_FIRST, USB_MUX_ARRAY)
};

/**
 * Define all USB muxes except roots e.g.
 * MAYBE_CONST struct usb_mux USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_mux_0 = {
 *         .usb_port = 0,
 *         .next_mux = NULL,
 *         .board_init = NULL,
 *         .board_set = NULL,
 *         .flags = 0,
 *         .driver = &virtual_usb_mux_driver,
 *         .hpd_update = &virtual_hpd_update,
 * };
 * MAYBE_CONST struct usb_mux USB_MUX_NODE_<node_id> = { ... };
 */
USB_MUX_FOREACH_USBC_PORT(USB_MUX_NO_FIRST, USB_MUX_DEFINE)

#endif /* #if DT_HAS_COMPAT_STATUS_OKAY */
