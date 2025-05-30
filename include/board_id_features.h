/*
 * Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_BOARD_ID_FEATURES_H
#define __EC_BOARD_BOARD_ID_FEATURES_H

#include "board_id.h"

/* The board id wasn't set when Cr50 booted */
#define BOARD_ID_FEATURES_UNSET_BID	BIT(0)
/* The board id features are initialized */
#define BOARD_ID_FEATURES_INITIALIZED	BIT(1)
/* Bits to store write protect bit state across deep sleep and resets. */
#define BOARD_ID_FWMP_BLOCK_DEV_RST_EC	BIT(2)

/* Print the enabled BID features */
void print_board_id_features(void);
/*
 * After reboot, read the board id, calculate the enabled features, and save
 * them in pwrdn scratch
 * Load the stored values after deep sleep.
 */
void init_board_id_features(void);
/*
 * Return true if the ID enables resetting the EC when the device enters rec+dev
 * and the FWMP is blocking dev mode.
 */
int bid_feature_id_resets_ec_in_recdev(const struct board_id *id);
int bid_feature_enabled_pcr_ecrst_recdev(void);
#endif  /* ! __EC_BOARD_BOARD_ID_FEATURES_H */
