/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/fff.h>

DECLARE_FAKE_VOID_FUNC(usb_mux_enable_alternative);

/* TODO: Remove this when board test is done. */
#ifdef CONFIG_ZTEST

#undef USB_MUX_ENABLE_ALTERNATIVE
#define USB_MUX_ENABLE_ALTERNATIVE(x) usb_mux_enable_alternative()

#endif /* CONFIG_ZTEST */
