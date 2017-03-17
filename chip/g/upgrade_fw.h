/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_CHIP_G_UPGRADE_FW_H
#define __EC_CHIP_G_UPGRADE_FW_H

#include <stddef.h>
#include "update_fw.h"

/*
 * This file contains structures used to facilitate cr50 firmware updates,
 * which can be used on any g chip.
 *
 * The firmware update protocol consists of two phases: connection
 * establishment and actual image transfer.
 *
 * Image transfer is done in 1K blocks. The host supplying the image
 * encapsulates blocks in frames by prepending a header including the flash
 * offset where the block is destined and its digest.
 *
 * The CR50 device responds to each frame with a confirmation which is 1 byte
 * response. Zero value means success, non zero value is the error code
 * reported by CR50.
 *
 * To establish the connection, the host sends a different frame, which
 * contains no data and is destined to offset 0. Receiving such a frame
 * signals the CR50 that the host intends to transfer a new image.
 *
 * The connection establishment response is described by the
 * first_response_pdu structure below.
 */

#define UPDATE_PROTOCOL_VERSION 6

/* Used to tell fw update the update ran successfully and is finished */
void fw_update_complete(void);

/* Verify integrity of the PDU received over USB. */
int usb_pdu_valid(struct update_command *cmd_body,
		  size_t cmd_size);

#endif  /* ! __EC_CHIP_G_UPGRADE_FW_H */
