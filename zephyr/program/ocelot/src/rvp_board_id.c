/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "drivers/rvp_board_id.h"
#include "hooks.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(rvp_board_id, LOG_LEVEL_INF);

/*
 * Returns board version on success, -1 on error.
 */
__override int board_get_version(void)
{
	/* Cache the board ID */
	static int rvp_board_id;

	int board_id = -1;
	int fab_id = -1;

	int reset_flags;

	/* Return ID, if already read */
	if (rvp_board_id)
		return rvp_board_id;

	/*
	 * If version request is received after a sysjump, retrieve id
	 * from CBI memory and save it to rvp_board_id.
	 */
	reset_flags = system_get_reset_flags();
	if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		if (!cbi_get_model_id(&rvp_board_id))
			return rvp_board_id;
	}

	/* read board_id */
	board_id = get_rvp_id_config(BOARD_ID);

	/* read fab id */
	fab_id = get_rvp_id_config(FAB_ID);

	rvp_board_id = board_id | (fab_id << 8);

	LOG_INF("board version: %d", rvp_board_id);

	/* Return the value, if CBI has stored it already. */
	if ((!cbi_get_model_id(&id_read))
			&& (rvp_board_id == id_read))
		return rvp_board_id;

	cbi_set_model_id(rvp_board_id);

	return rvp_board_id;
}
