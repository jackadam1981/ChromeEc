/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helpers to emulate a U2F HID dongle over the TPM transport */

#include "console.h"
#include "extension.h"
#include "pop.h"
#include "rbox.h"
#include "registers.h"
#include "task.h"
#include "tpm_vendor_cmds.h"
#include "timer.h"
#include "u2f_corp.h"
#include "u2fhid_corp.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)

/* Use the laptop power button as a physical presence */

static timestamp_t last_press;

/* how long do we keep the last button press as valid presence */
#define PRESENCE_TIMEOUT (10 * SECOND)

void power_button_record(void)
{
	if (rbox_powerbtn_is_pressed())
		last_press = get_time();
}

enum touch_state pop_check_presence(int consume)
{
	int recent = (get_time().val - last_press.val) < PRESENCE_TIMEOUT;

	CPRINTS("Presence:%d\n", recent);
	if (consume)
		last_press.val = 0;

	/* user physical presence on the power button */
	return recent ? POP_TOUCH_YES : POP_TOUCH_NO;
}

/* Send/receive U2F APDU over TPM vendor commands */

enum vendor_cmd_rc vc_u2f(enum vendor_cmd_cc code, void *body,
			  size_t cmd_size, size_t *response_size)
{
	uint16_t retlen;
	static uint8_t tmp_tx_buf[MAX_BCNT];

	/* Call U2F code in CR52 */
	retlen = apdu_rcv((const uint8_t *)body, cmd_size, tmp_tx_buf);
	if (retlen)
		memcpy(body, tmp_tx_buf, retlen);

	*response_size = retlen;
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2F, vc_u2f);
