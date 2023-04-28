/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "string.h"
#include "task.h"

#include <zephyr/kernel.h>

struct host_command *zephyr_find_host_command(int command)
{
	STRUCT_SECTION_FOREACH(host_command, cmd)
	{
		if (cmd->command == command)
			return cmd;
	}

	return NULL;
}

void host_command_main(void)
{
	k_thread_priority_set(get_main_thread(),
			      EC_TASK_PRIORITY(EC_TASK_HOSTCMD_PRIO));
	k_thread_name_set(get_main_thread(), "HOSTCMD");
	host_command_task(NULL);
}

#ifdef CONFIG_EC_HOST_CMD
static enum ec_status
host_command_get_cmd_versions(struct ec_host_cmd_handler_args *args)
{
	const struct ec_host_cmd_handler *found_handler = NULL;
	const struct ec_params_get_cmd_versions *p = args->input_buf;
	const struct ec_params_get_cmd_versions_v1 *p_v1 = args->input_buf;
	struct ec_response_get_cmd_versions *r = args->output_buf;

	memset(r, 0, sizeof(*r));
	STRUCT_SECTION_FOREACH(ec_host_cmd_handler, handler)
	{
		int searched_id = (args->version == 1) ? p_v1->cmd : p->cmd;

		if (handler->id == searched_id) {
			found_handler = handler;
			break;
		}
	}

	if (!found_handler)
		return EC_RES_INVALID_PARAM;

	r->version_mask = found_handler->version_mask;

	args->output_buf_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
EC_HOST_CMD_HANDLER(EC_CMD_GET_CMD_VERSIONS, host_command_get_cmd_versions,
		    EC_VER_MASK(0) | EC_VER_MASK(1),
		    struct ec_params_get_cmd_versions,
		    struct ec_response_get_cmd_versions);
#endif
