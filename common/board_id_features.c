/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "registers.h"
#include "board_id.h"
#include "board_id_features.h"
#include "console.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

uint32_t board_id_features;

static void load_board_id_features(void)
{
	board_id_features = GREG32(PMU, PWRDN_SCRATCH25);
}

void print_board_id_features(void)
{
	/* Note, this includes BOARD_CFG reg in top 16 bits. */
	ccprintf("bid features = %08x\n", board_id_features);
}

int bid_feature_enabled_pcr_ecrst_recdev(void)
{
	/* If the board id is unset, don't enable the feature */
	if (board_id_features & BOARD_ID_FEATURES_UNSET_BID)
		return 0;
	/*
	 * If the BID features aren't initialized, there was some bid error
	 * enable the feature to be safe.
	 */
	if (!(board_id_features & BOARD_ID_FEATURES_INITIALIZED))
		return 1;
	return !!(board_id_features & BOARD_ID_FWMP_BLOCK_DEV_RST_EC);
}

void init_board_id_features(void)
{
	struct board_id id;
	uint32_t features = 0;

	/*
	 * BID features are stored in PWRDN scratch. They are saved through
	 * deep sleep. They are reinitialized after every other type of reset.
	 */
	load_board_id_features();
	if (board_id_features & BOARD_ID_FEATURES_INITIALIZED) {
		CPRINTS("restore BID features");
		return;
	}
	store_board_id_features(0);
	CPRINTS("init BID features");
	/*
	 * If there's a board id mismatch, reset the EC in rec+dev when the FWMP
	 * is blocking dev mode.
	 */
	if (board_id_is_mismatched())
		return;

	/*
	 * If cr50 can't read the board id for some reason, return true just to
	 * be safe.
	 */
	if (read_board_id(&id) != EC_SUCCESS) {
		CPRINTS("%s: BID read error", __func__);
		return;
	}

	if (board_id_is_blank(&id)) {
		store_board_id_features(BOARD_ID_FEATURES_UNSET_BID);
		return;
	}

#ifdef CONFIG_FWMP_BLOCK_REC_DEV_RESET_EC
	if (bid_feature_id_resets_ec_in_recdev(&id))
		features |= BOARD_ID_FWMP_BLOCK_DEV_RST_EC;
#endif
	store_board_id_features(features | BOARD_ID_FEATURES_INITIALIZED);
}
