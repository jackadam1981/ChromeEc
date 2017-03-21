/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "console.h"
#include "endian.h"
#include "extension.h"
#include "flash.h"
#include "flash_info.h"
#include "hooks.h"
#include "signed_header.h"
#include "system.h"
#include "upgrade_fw.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

static void deferred_reboot(void)
{
	system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED | SYSTEM_RESET_HARD);
}
DECLARE_DEFERRED(deferred_reboot);

#define MAX_REBOOT_TIMEOUT_MS 1000

static int header_restored(uint32_t offset)
{
	volatile struct SignedHeader *header;
	uint32_t new_size;

	header = (volatile struct SignedHeader *)
		(CONFIG_PROGRAM_MEMORY_BASE + offset);

	new_size = header->image_size;
	if (!(new_size & TOP_IMAGE_SIZE_BIT))
		return 0;

	new_size &= ~TOP_IMAGE_SIZE_BIT;
	/*
	 * Clear only in case the size is sensible (i.e. not set to all
	 * ones).
	 */
	if (new_size > CONFIG_RW_SIZE)
		return 0;

	if ((offset == CONFIG_RO_MEM_OFF) || (offset == CHIP_RO_B_MEM_OFF))
		flash_open_ro_window(offset, sizeof(struct SignedHeader));

	return flash_physical_write(offset + offsetof(struct SignedHeader,
						      image_size),
				    sizeof(header->image_size),
				    (char *)&new_size) == EC_SUCCESS;
}

static uint8_t headers_restored(void)
{
	uint8_t total_restored;

	/* Examine the RO first. */
	if (system_get_ro_image_copy() == SYSTEM_IMAGE_RO)
		total_restored = header_restored(CHIP_RO_B_MEM_OFF);
	else
		total_restored = header_restored(CONFIG_RO_MEM_OFF);

	/* Now the RW */
	if (system_get_image_copy() == SYSTEM_IMAGE_RW)
		total_restored += header_restored(CONFIG_RW_B_MEM_OFF);
	else
		total_restored += header_restored(CONFIG_RW_MEM_OFF);

	return total_restored;
}

/*
 * The TURN_UPDATE_ON command comes with a single parameter, which is an 16
 * bit integer value of number of milliseconds to wait before reboot in case
 * there has been an update waiting.
 *
 * Maximum wait time is 1000 ms.
 *
 * Return value send to the host is a single byte, 0 if there is no update
 * waiting and 1 if there is an update waiting.
 *
 * If there is an update waiting AND the requested time exceeds 1000 ms an
 * error code is returned.
 */
static enum vendor_cmd_rc turn_update_on(enum vendor_cmd_cc code,
					 void *buf,
					 size_t input_size,
					 size_t *response_size)
{
	uint16_t timeout;
	uint8_t *response;

	/* Just in case. */
	*response_size = 0;

	if (input_size < sizeof(uint16_t)) {
		CPRINTF("%s: incorrect request size %d\n",
			__func__, input_size);
		return VENDOR_RC_BOGUS_ARGS;
	}

	/* Retrieve the requested timeout. */
	memcpy(&timeout, buf, sizeof(timeout));
	timeout = be16toh(timeout);

	if (timeout > MAX_REBOOT_TIMEOUT_MS) {
		CPRINTF("%s: incorrect timeout value %d\n",
			__func__, timeout);
		return VENDOR_RC_BOGUS_ARGS;
	}

	*response_size = 1;
	response = buf;

	*response = headers_restored();
	if (*response && timeout) {
		/*
		 * At least one header was restored, and timeout is not zero,
		 * set up the reboot.
		 */
		CPRINTF("%s: rebooting in %d ms\n", __func__, timeout);
		hook_call_deferred(&deferred_reboot_data, timeout * MSEC);
	}

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_TURN_UPDATE_ON, turn_update_on);

/* This command's implementation is shared with USB updater. */
DECLARE_EXTENSION_COMMAND(EXTENSION_FW_UPGRADE, fw_upgrade_command_handler);
