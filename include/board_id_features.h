/*
 * Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_BOARD_ID_FEATURES_H
#define __EC_BOARD_BOARD_ID_FEATURES_H

#include "board_id.h"

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
#endif  /* ! __EC_BOARD_BOARD_ID_FEATURES_H */
