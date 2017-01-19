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
#include "u2fhid.h"
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

/* Send/receive U2F HID frames over TPM vendor commands */

#define TPM_EVENT_U2F   TASK_EVENT_CUSTOM(1 << 16)
#define TPM_EVENT_CONT  TASK_EVENT_CUSTOM(1 << 15)
#define TPM_EVENT_MASK  (TPM_EVENT_U2F | TPM_EVENT_CONT)

#define U2F_EVENT_FRAME TASK_EVENT_CUSTOM(1)
#define U2F_EVENT_CONT  TASK_EVENT_CUSTOM(2)

static uint8_t *current_frame;

void usbu2f_get_frame(U2FHID_FRAME *frame_p)
{
	if (current_frame)
		memcpy(frame_p, current_frame, U2F_REPORT_SIZE);
	else
		CPRINTS("ERR: empty frame");
}

void usbu2f_put_frame(const U2FHID_FRAME *f_p)
{
	if (!f_p) { /* wait for continuation first */
		/* send an empty answer, we are waiting for TX cont... */
		task_set_event(TASK_ID_TPM, TPM_EVENT_CONT, 0);
		return;
	}

	while (!current_frame) {
		/* continuation: wait for the next frame from the host */
		if (task_wait_event_mask(U2F_EVENT_CONT,
					 U2FHID_TRANS_TIMEOUT * MSEC)
							== TASK_EVENT_TIMER) {
			return;
		}
	}
	memcpy(current_frame, f_p, U2F_REPORT_SIZE);
	current_frame = NULL;
	task_set_event(TASK_ID_TPM, TPM_EVENT_U2F, 0);
}

enum vendor_cmd_rc vc_u2fhid_report(enum vendor_cmd_cc code, void *body,
				    size_t cmd_size,
				    size_t *response_size)
{
	uint32_t evt;

	if (cmd_size == 0) {
		current_frame = body;
		/* Wake-up the U2F task to send the next CONT */
		task_set_event(TASK_ID_U2F, U2F_EVENT_CONT, 0);
		*response_size = U2F_REPORT_SIZE;
		return VENDOR_RC_SUCCESS;
	}

	if (cmd_size && cmd_size != U2F_REPORT_SIZE) {
		CPRINTS("Invalid U2F report %d\n", cmd_size);
		return VENDOR_RC_BOGUS_ARGS;
	}
	current_frame = body;
	/* Wake-up the U2F task to fetch the frame or get continuation */
	task_set_event(TASK_ID_U2F, cmd_size ? U2F_EVENT_FRAME
					     : U2F_EVENT_CONT, 0);
	/* Wait for the U2F task to produce the report for the answer */
	evt = task_wait_event_mask(TPM_EVENT_MASK, U2FHID_TRANS_TIMEOUT * MSEC);
	if (evt == TASK_EVENT_TIMER) {
		current_frame = NULL;
		*response_size = 0;
		return VENDOR_RC_TIMEOUT;
	}

	*response_size = evt == TPM_EVENT_CONT ? 0 : U2F_REPORT_SIZE;
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2FHID_REPORT, vc_u2fhid_report);

/* various chip id accessors for the X509 common code: TODO rework */
uint32_t chip_DEV_ID0(void) { return GREG32(FUSE, DEV_ID0); }
uint32_t chip_DEV_ID1(void) { return GREG32(FUSE, DEV_ID1); }

uint16_t chip_category(void) {
	uint32_t reg = GREG32(PMU, CHIP_ID);

	switch ((reg >> 28) & 0x0f) {
	case 0x03: /* B1 */
		return 0x00;
	case 0x04: // B2
		return 0x01;
	case 0x01: // FPGA
		return 0xFFFF;
	default:
		return 0x00;
	}
}

