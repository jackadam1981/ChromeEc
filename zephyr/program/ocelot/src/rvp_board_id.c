/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "drivers/rvp_board_id.h"
#include "hooks.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(rvp_board_id, LOG_LEVEL_DBG);

static int rvp_board_id = -1;

void ocelotrvp_id_handler(void)
{
	int board_id;
	int fab_id;

	board_id = get_rvp_id_config(BOARD_ID);
	if (board_id < 0) {
		LOG_DBG("RVP_ID: get_rvp_id_config failed");
	}

	fab_id = get_rvp_id_config(FAB_ID);
	if (fab_id < 0) {
		LOG_DBG("RVP_ID: get_rvp_id_config failed");
	}

	rvp_board_id = board_id | (fab_id << 8);

	LOG_DBG("RVP_ID: %d from driver", rvp_board_id);

	uint32_t id_from_cbi;
	if ((cbi_get_model_id(&id_from_cbi) == EC_SUCCESS) &&
	    id_from_cbi == rvp_board_id) {

		LOG_DBG("RVP_ID: %d matches CBI", rvp_board_id);

		return;
	}

	LOG_DBG("RVP_ID: %d store in CBI ", rvp_board_id);
	cbi_set_model_id(rvp_board_id);
}

/*
 * Returns board version on success, -1 on error.
 */
__override int board_get_version(void)
{
	int id;

	if (rvp_board_id != -1) {
		LOG_DBG("RVP_ID: %d (cached)", rvp_board_id);
		return rvp_board_id;
	}

	if (cbi_get_model_id(&id) == EC_SUCCESS) {
		LOG_DBG("RVP_ID: %d load from CBI", id);
		rvp_board_id = id;
		return id;
	}

	LOG_DBG("RVP_ID: not available");
	return -1;
}

