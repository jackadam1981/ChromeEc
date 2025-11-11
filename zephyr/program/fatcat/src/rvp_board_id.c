/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "drivers/rvp_board_id.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(rvp_model_id, LOG_LEVEL_DBG);

#define FAB_ID_SHIFT 8
#define BOARD_ID_MASK (BIT(FAB_ID_SHIFT) - 1)

/*
 * rvp_model_id is composed of all the interesting RVP ID GPIOs.
 * Only FAB and board ID GPIOs are presenty used;
 * BOM GPIOs are ignored.
 */
static int rvp_model_id = -1;

void fatcatrvp_id_handler(void)
{
	int board_id;
	int fab_id;

	board_id = get_rvp_id_config(BOARD_ID);
	if (board_id < 0) {
		LOG_DBG("RVP_ID: get_rvp_id_config.BOARD_ID failed");
		return;
	}

	fab_id = get_rvp_id_config(FAB_ID);
	if (fab_id < 0) {
		LOG_DBG("RVP_ID: get_rvp_id_config.FAB_ID failed");
		return;
	}

	rvp_model_id = (fab_id << FAB_ID_SHIFT) | board_id;

	LOG_DBG("RVP_ID: %d from driver", rvp_model_id);

	uint32_t id_from_cbi;
	if ((cbi_get_model_id(&id_from_cbi) == EC_SUCCESS) &&
	    id_from_cbi == rvp_model_id) {
		// CBI MODEL_ID up-to-date, we're done
		LOG_DBG("RVP_ID: %d matches CBI", rvp_model_id);
		return;
	}

	LOG_INF("RVP_ID: %d store in CBI", rvp_model_id);
	cbi_set_model_id(rvp_model_id);
}

/*
 * Returns board version on success, -1 on error.
 */
__override int board_get_version(void)
{
	if (rvp_model_id == -1) {
		int id;

		if (cbi_get_model_id(&id) != EC_SUCCESS) {
			LOG_INF("RVP_ID: not available");
			return -1;
		}

		LOG_DBG("RVP_ID: %d load from CBI", id);
		rvp_model_id = id;
	}

	return rvp_model_id & BOARD_ID_MASK;
}
