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

/** usb_muxes is constant only if TCPC configuration cannot change in runtime */
#define MAYBE_CONST COND_CODE_1(CONFIG_PLATFORM_EC_USB_PD_TCPC_RUNTIME_CONFIG, \
				(), (const))

/** Define root of each USB muxes chain */
MAYBE_CONST struct usb_mux usb_muxes[] = {
	USB_MUX_FOREACH_USBC_PORT(USB_MUX_FIRST, USB_MUX_ARRAY)
};

/** Define all USB muxes expect roots */
USB_MUX_FOREACH_USBC_PORT(USB_MUX_NO_FIRST, USB_MUX_DEFINE)

#endif /* #if DT_HAS_COMPAT_STATUS_OKAY */
