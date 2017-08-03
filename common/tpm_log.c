/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "extension.h"
#include "host_command.h"
#include "timer.h"
#include "tpm_log.h"
#include "tpm_vendor_cmds.h"
#include "usb_pd.h"
#include "util.h"

#define TPM_EVENT_LOG_SIZE 8

void tpm_log_event(enum tpm_event type, uint16_t data)
{
	uint32_t timestamp = get_time().val >> PD_LOG_TIMESTAMP_SHIFT;

	log_add_event(type, 0, data, NULL, timestamp);
}

static enum vendor_cmd_rc vc_get_log_entry(enum vendor_cmd_cc code,
					   void *buf,
					   size_t input_size,
					   size_t *response_size)
{
	struct event_log_entry *entry = buf;
	int byte_size = log_dequeue_event(entry);

	if (entry->type == EVENT_LOG_NO_ENTRY)
		return VENDOR_RC_DATA_NOT_AVAILABLE;

	ASSERT(byte_size == TPM_EVENT_LOG_SIZE);
	*response_size = byte_size;
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_GET_LOG_ENTRY, vc_get_log_entry);
