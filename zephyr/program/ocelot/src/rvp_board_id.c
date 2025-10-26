/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/rvp_board_id.h"
#include "hooks.h"

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(rvp_board_id, LOG_LEVEL_INF);

/* Cache the board ID */
static int rvp_board_id;

#define RVP_SKUID_SYSJUMP_TAG 0x5253 /* RS */
#define RVP_SKUID_HOOK_VERSION 1

/*
 * Returns board version on success, -1 on error.
 */
__override int board_get_version(void)
{
	int board_id = -1;
	int fab_id = -1;

	/* Board ID is already read */
	if (rvp_board_id)
		return rvp_board_id;

	/* read board_id */
	board_id = get_rvp_id_config(BOARD_ID);

	/* read fab id */
	fab_id = get_rvp_id_config(FAB_ID);

	rvp_board_id = board_id | (fab_id << 8);

	LOG_INF("board version: %d", rvp_board_id);

	return rvp_board_id;
}

/*
 * Preserve RVP SKUID across a sysjump.
 */

static void rvp_sku_id_preserve_state(void)
{
	system_add_jump_tag(RVP_SKUID_SYSJUMP_TAG, RVP_SKUID_HOOK_VERSION,
			    sizeof(rvp_board_id), &rvp_board_id);
}
DECLARE_HOOK(HOOK_SYSJUMP, rvp_sku_id_preserve_state, HOOK_PRIO_DEFAULT);

/*
 * Restore RVP SKUID after a sysjump.
 */
static void rvp_sku_id_restore_state(void)
{
	const uint32_t *prev_rvp_board_id;
	int size, version;

	prev_rvp_board_id = (const uint32_t *)system_get_jump_tag(
		RVP_SKUID_SYSJUMP_TAG, &version, &size);

	if (prev_rvp_board_id && version == RVP_SKUID_HOOK_VERSION &&
	    size == sizeof(prev_rvp_board_id)) {
		memcpy(&rvp_board_id, prev_rvp_board_id, sizeof(rvp_board_id));
		LOG_INF("Restored BID = %d", rvp_board_id);
	}
}
DECLARE_HOOK(HOOK_INIT, rvp_sku_id_restore_state, HOOK_PRIO_DEFAULT);
