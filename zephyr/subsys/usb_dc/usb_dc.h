/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB DC Shimming Definitions.
 */

#ifndef __USB_DC_H
#define __USB_DC_H

#include "common.h"
#include "hooks.h"

#include <zephyr/usb/usb_ch9.h>

#if defined(CONFIG_CROS_EC_RW)
extern  __maybe_unused const struct deferred_data kb_resume_deferred_data;
#endif

bool check_usb_is_suspended(void);
bool check_usb_is_configured(void);

/**
 * @brief Request usb wake-up
 *
 * @return true if wake up successfully, false otherwise
 */
bool request_usb_wake(void);

#endif
