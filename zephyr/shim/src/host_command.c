/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "string.h"
#include "task.h"

#include <zephyr/kernel.h>
#include <zephyr/mgmt/ec_host_cmd/ec_host_cmd.h>
#include <zephyr/sys/iterable_sections.h>

#ifndef CONFIG_ZTEST
#if defined(CONFIG_EC_HOST_CMD) &&                    \
	(!defined(CONFIG_TASK_HOSTCMD_THREAD_MAIN) || \
	 defined(CONFIG_EC_HOST_CMD_DEDICATED_THREAD))
BUILD_ASSERT(0, "The upstream Host Command subsystem is supported only with "
		"reusing the main thread.");
#endif
#endif /* CONFIG_ZTEST */

struct host_command *zephyr_find_host_command(int command)
{
	STRUCT_SECTION_FOREACH(host_command, cmd)
	{
		if (cmd->command == command)
			return cmd;
	}

	return NULL;
}

#ifdef CONFIG_EC_HOST_CMD
static void ec_host_cmd_print(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vcprints(CC_HOSTCMD, format, args);
	va_end(args);
}
#endif /* CONFIG_EC_HOST_CMD */

void host_command_main(void)
{
	k_thread_priority_set(get_main_thread(),
			      EC_TASK_PRIORITY(EC_TASK_HOSTCMD_PRIO));
	k_thread_name_set(get_main_thread(), "HOSTCMD");
#ifndef CONFIG_EC_HOST_CMD
	host_command_task(NULL);
#else
	/* Install a function to log HC communication. Use cprints. */
	ec_host_cmd_log_install(ec_host_cmd_print);
	ec_host_cmd_task();
#endif
}

#ifdef CONFIG_EC_HOST_CMD
DECLARE_HOOK(HOOK_INIT, host_command_init, HOOK_PRIO_DEFAULT);
#endif

#ifdef CONFIG_EC_HOST_CMD
static enum ec_host_cmd_status
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
		return EC_HOST_CMD_INVALID_PARAM;

	r->version_mask = found_handler->version_mask;

	args->output_buf_size = sizeof(*r);

	return EC_HOST_CMD_SUCCESS;
}
EC_HOST_CMD_HANDLER(EC_CMD_GET_CMD_VERSIONS, host_command_get_cmd_versions,
		    EC_VER_MASK(0) | EC_VER_MASK(1),
		    struct ec_params_get_cmd_versions,
		    struct ec_response_get_cmd_versions);
#endif
