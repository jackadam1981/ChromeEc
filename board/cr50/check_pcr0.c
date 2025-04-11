/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board_id.h"
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)


#define FWMP_DOES_NOT_RESET_EC_IN_REC_DEV 1
#define FWMP_RESETS_EC_IN_REC_DEV 2

/*
 * Certain boards need to reset the EC if the FWMP is blocking dev mode and the
 * device tries to enter rec+dev.
 * Enable this on certain boards based on the board id type.
 */
#define FWMP_EC_RESET_ALLOWLIST_COUNT 1
/*
 * This contains the FWMP reset EC in rec+dev allowlist. Reset the EC if the
 * FWMP has block dev mode set and the device tries to enter rec+dev mode when
 * the board id type is found in this list.
 */
const uint32_t fwmp_reset_ec_in_rec_dev_allowlist[] = {
	0x12345678, /* tast test value. Not a real RLZ. Delete this if a real */
		    /* RLZ is added */
};
BUILD_ASSERT(ARRAY_SIZE(fwmp_reset_ec_in_rec_dev_allowlist) ==
		FWMP_EC_RESET_ALLOWLIST_COUNT);

int board_id_fwmp_resets_ec_in_rec_dev(void)
{
	static int checked_fwmp_ec_reset;
	struct board_id id;
	int i;

	/*
	 * If there's a board id mismatch, reset the EC in rec+dev when the FWMP
	 * is blocking dev mode.
	 */
	if (board_id_is_mismatched())
		return true;

	if (checked_fwmp_ec_reset)
		return checked_fwmp_ec_reset == FWMP_RESETS_EC_IN_REC_DEV;

	/*
	 * If cr50 can't read the board id for some reason, return true just to
	 * be safe.
	 */
	if (read_board_id(&id) != EC_SUCCESS) {
		CPRINTS("%s: BID read error", __func__);
		return true;
	}

	if (board_id_is_blank(&id))
		return false;

	/*
	 * Cache the board id fwmp resets EC state, so cr50 doesn't need to keep
	 * reading and checking the RLZ. The board id can't change if it's
	 * already set.
	 */
	checked_fwmp_ec_reset = FWMP_DOES_NOT_RESET_EC_IN_REC_DEV;
	for (i = 0; i < ARRAY_SIZE(fwmp_reset_ec_in_rec_dev_allowlist); i++) {
		if (id.type == fwmp_reset_ec_in_rec_dev_allowlist[i]) {
			checked_fwmp_ec_reset = FWMP_RESETS_EC_IN_REC_DEV;
			break;
		}
	}
	CPRINTS("%s: caching FWMP EC reset state %d", __func__,
		checked_fwmp_ec_reset);
	return checked_fwmp_ec_reset == FWMP_RESETS_EC_IN_REC_DEV;
}
