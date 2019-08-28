/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "console.h"
#include "double_tap.h"
#include "ec_commands.h"
#include "mkbp_event.h"

#define CPRINTS(format, args...) cprints(CC_MOTION_LID, format, ## args)

#ifdef CONFIG_MKBP_EVENT
static int double_tap_get_next_event(uint8_t *data)
{
	return EC_SUCCESS;
}
DECLARE_EVENT_SOURCE(EC_MKBP_EVENT_DOUBLE_TAP, double_tap_get_next_event);

void double_tap_entry(void)
{
	CPRINTS("Notifying AP of double tap Entry...");
	mkbp_send_event(EC_MKBP_EVENT_DOUBLE_TAP);
}
#endif /* CONFIG_MKBP_EVENT */
