/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>

#include "common.h"
#include "system.h"
#include "ec_commands.h"
#include "host_command.h"

static char *test_buffer = "hello world";

static enum ec_status
hc_get_rw_safe_mode_info(struct host_cmd_handler_args *args)
{
	struct ec_response_rw_safe_mode_info *r = args->response;

	if (!system_is_in_rw_safe_mode()) {
		r->flags = 0;
		return EC_SUCCESS;
	}

	r->flags |= RW_SAFE_MODE_FLAG_IN_SAFE_MODE;

	r->dump_base_address = CONFIG_RAM_BASE;

	/* Ensure all entries are zero'd */
	for (int i = 0; i < ARRAY_SIZE(r->dump_entries); i++) {
		r->dump_entries[i].offset = 0;
		r->dump_entries[i].size = 0;
	}

	/* TODO: Replace this with compile time list of memory regions to save
	 */
	r->dump_entries[0].offset =
		((uint32_t)test_buffer) - r->dump_base_address;
	r->dump_entries[0].size = strlen(test_buffer);

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_RW_SAFE_MODE_INFO, hc_get_rw_safe_mode_info,
		     EC_VER_MASK(0));

static enum ec_status
hc_read_rw_safe_mode_mem(struct host_cmd_handler_args *args)
{
	const struct ec_params_rw_safe_mode_mem *p = args->params;
	struct ec_response_rw_safe_mode_mem *r = args->response;

	/* TODO: Verify that memory is within bounds of a safe mode memory
	 * region
	 */
	memcpy(r->mem, (const void *)((uint32_t)CONFIG_RAM_BASE + p->offset),
	       p->size);

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_READ_RW_SAFE_MODE_MEM, hc_read_rw_safe_mode_mem,
		     EC_VER_MASK(0));

static enum ec_status hc_finish_rw_safe_mode(struct host_cmd_handler_args *args)
{
	const struct ec_params_rw_safe_mode_status *p = args->params;

	system_reset(SYSTEM_RESET_HARD);

	/* Should never reach this code! */

	return EC_ERROR_UNKNOWN;
}
DECLARE_HOST_COMMAND(EC_CMD_FINISH_RW_SAFE_MODE, hc_finish_rw_safe_mode,
		     EC_VER_MASK(0));
