/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board_id.h"
#include "console.h"
#include "system.h"
#include "update_fw.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

/* Versions we have sent to GUC */
const struct signed_header_version guc_1 = {
	.epoch = 0,
	.major = 0,
	.minor = 13,
};
const struct signed_header_version guc_2 = {
	.epoch = 0,
	.major = 0,
	.minor = 22,
};


static int board_id_is_erased(void)
{
	struct board_id id;
	/*
	 * If we can't read the board id for some reason, return 0 just to be
	 * safe
	 */
	if (read_board_id(&id) != EC_SUCCESS) {
		CPRINTS("%s: error reading Board ID", __func__);
		return 0;
	}

	/* If all of the fields are all 0xffffffff, the board id is not set */
	if (~(id.type & id.type_inv & id.flags) == 0) {
		CPRINTS("Board id is erased");
		return 1;
	}
	return 0;
}

/* Returns True ver is the header version */
static int header_has_version(const struct SignedHeader *h,
			       const struct signed_header_version ver)
{
	return (ver.epoch == h->epoch_ && ver.major == h->major_ &&
		ver.minor == h->minor_);
}

static int inactive_image_is_guc_image(void)
{
	enum system_image_copy_t inactive_copy;
	const struct SignedHeader *other;

	if (system_get_image_copy() == SYSTEM_IMAGE_RW_A)
		inactive_copy = SYSTEM_IMAGE_RW_B;
	else
		inactive_copy = SYSTEM_IMAGE_RW_A;
	other = (struct SignedHeader *) get_program_memory_addr(
		inactive_copy);
	if (header_has_version(other, guc_1) ||
	    header_has_version(other, guc_2)) {
		CPRINTS("Inactive image is from GUC");
		return 1;
	}
	/*
	 * TODO(mruthven): Return true if factory image field of header is
	 * set
	 */
	return 0;
}

/**
 * Return non-zero if this is the first boot of a board in the factory.
 *
 * This is used to determine whether the default CCD configuration will be RMA
 * (things are unlocked for factory) or normal (things locked down because not
 * in factory).
 *
 * checks:
 * - If the board ID exists, this is not the first boot
 * - If the inactive image is not a GUC image, then we've left the factory
 */
int board_is_first_factory_boot(void)
{
	return inactive_image_is_guc_image() && board_id_is_erased();
}
