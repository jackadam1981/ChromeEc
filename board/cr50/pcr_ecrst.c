/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board_id.h"
#include "board_id_features.h"
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#ifndef CONFIG_BOARD_ID_FEATURES
#error "must enable BOARD_ID_FEATURES to enable FWMP_BLOCK_REC_DEV_RESET_EC"
#endif

/*
 * Certain boards need to reset the EC if the FWMP is blocking dev mode and the
 * device tries to enter rec+dev.
 * Enable this on certain boards based on the board id type.
 */
#define CHECK_PCR_FWMP_ECRST_ALLOW_LIST_COUNT 1
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
		CHECK_PCR_FWMP_ECRST_ALLOW_LIST_COUNT);

/*
 * Checks if the board id is allowed to reset the EC when the FWMP is blocking
 * dev mode and the device tries to enter rec+dev.
 *
 * Returns:
 *   true if the board id is allowed to reset the EC.
 *   false if the board id is not allowed to reset the EC.
 */
int bid_feature_id_resets_ec_in_recdev(const struct board_id *id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fwmp_reset_ec_in_rec_dev_allowlist); i++) {
		if (id->type == fwmp_reset_ec_in_rec_dev_allowlist[i]) {
			CPRINTS("bid feature: reset EC in rec+dev");
			return 1;
		}
	}
	return 0;
}
