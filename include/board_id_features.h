/*
 * Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_BOARD_ID_FEATURES_H
#define __EC_BOARD_BOARD_ID_FEATURES_H

#include "board_id.h"
#include "registers.h"

/* The board id wasn't set when Cr50 booted */
#define BOARD_ID_FEATURES_UNSET_BID BIT(0)

/* The board id features are initialized */
#define BOARD_ID_FEATURES_INITIALIZED BIT(1)

/* Bits to store write protect bit state across deep sleep and resets. */
#define BOARD_ID_FWMP_BLOCK_DEV_RST_EC BIT(2)

/* Bits to store wherever Strongbox was explicitly enabled */
#define BOARD_ID_FEATURES_SB_ENABLE_SET BIT(3)

/* Bits to store wherever Strongbox was explicitly disabled */
#define BOARD_ID_FEATURES_SB_DISABLE_SET BIT(4)

/**
 * We may have CONFIG_BOARD_ID_FEATURES disabled for the part related with
 * Board Id flags, but still have Board Id features register available.
 */

/**
 * Set Board Id features in PWRDN_SCRATCH25, and, if configured in shadow copy.
 * @param features features to set
 */
static inline void store_board_id_features(uint32_t features)
{
#ifdef CONFIG_BOARD_ID_FEATURES
	extern uint32_t board_id_features;

	board_id_features = features;
#endif
	GREG32(PMU, PWRDN_SCRATCH25) = features;
}

/**
 * Get Board Id features.
 * @returns bitmask of board id features
 */
static inline uint32_t get_board_id_features(void)
{
#ifdef CONFIG_BOARD_ID_FEATURES
	extern uint32_t board_id_features;
	return board_id_features;
#else
	return GREG32(PMU, PWRDN_SCRATCH25);
#endif
}

/**
 * Add features to Board id features.
 * @param features
 */
static inline void add_board_id_features(uint32_t features)
{
	store_board_id_features(get_board_id_features() | features);
}

/* Print the enabled BID features */
void print_board_id_features(void);
/*
 * After reboot, read the board id, calculate the enabled features, and save
 * them in pwrdn scratch
 * Load the stored values after deep sleep.
 */
void init_board_id_features(void);

/*
 * Checks if the given board id is allowed to reset the EC when the FWMP is
 * blocking dev mode and the device tries to enter rec+dev.
 *
 * Returns:
 *   true if the board id is allowed to reset the EC.
 *   false if the board id is not allowed to reset the EC.
 */
int bid_feature_id_resets_ec_in_recdev(const struct board_id *id);

/*
 * Checks the cached board id features to see if the chip board id is allowed
 * to reset the EC when the FWMP is blocking dev mode and the device tries to
 * enter rec+dev.
 *
 * Returns:
 *   true if the board id is allowed to reset the EC.
 *   false if the board id is not allowed to reset the EC.
 */
int bid_feature_enabled_pcr_ecrst_recdev(void);
#endif /* ! __EC_BOARD_BOARD_ID_FEATURES_H */
