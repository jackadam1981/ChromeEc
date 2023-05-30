/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_hang_detect.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "string.h"
#include "task.h"

#include <zephyr/kernel.h>
#include <zephyr/mgmt/ec_host_cmd/ec_host_cmd.h>
#include <zephyr/sys/iterable_sections.h>

#ifdef CONFIG_EC_HOST_CMD
#ifndef CONFIG_ZTEST
#if !defined(CONFIG_TASK_HOSTCMD_THREAD_MAIN) || \
	defined(CONFIG_EC_HOST_CMD_DEDICATED_THREAD)
BUILD_ASSERT(0, "The upstream Host Command subsystem is supported only with "
		"reusing the main thread.");
#endif
#else
#if (defined(CONFIG_TASK_HOSTCMD_THREAD_DEDICATED) &&  \
     !defined(CONFIG_EC_HOST_CMD_DEDICATED_THREAD)) || \
	(defined(CONFIG_TASK_HOSTCMD_THREAD_MAIN) &&   \
	 defined(CONFIG_EC_HOST_CMD_DEDICATED_THREAD))
BUILD_ASSERT(0, "Enable/disable upstream Host Command dedicated thread.");
#endif
#endif /* CONFIG_ZTEST */
#endif /* CONFIG_EC_HOST_CMD */

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
static void ec_host_cmd_user_cb(const struct ec_host_cmd_rx_ctx *rx_ctx,
				void *user_data)
{
	const struct ec_host_cmd_request_header *const rx_header =
		(void *)rx_ctx->buf;

	/*
	 * If this is the reboot command, reboot immediately. This gives the
	 * host processor a way to unwedge the EC even if it's busy with some
	 * other command.
	 */
	if (rx_header->cmd_id == EC_CMD_REBOOT) {
		system_reset(SYSTEM_RESET_HARD);
	}

#ifdef CONFIG_AP_HANG_DETECT
	/* If hang detection is enabled, check stop on host command */
	hang_detect_stop_on_host_command();
#endif
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
#ifndef CONFIG_EC_HOST_CMD_DEDICATED_THREAD
	ec_host_cmd_task();
#endif /* CONFIG_EC_HOST_CMD_DEDICATED_THREAD */
#endif
}

#ifdef CONFIG_EC_HOST_CMD
void host_command_install_cb(void)
{
	/* Set the user callback for custom EC procedures */
	ec_host_cmd_set_user_cb(ec_host_cmd_user_cb, NULL);
}
DECLARE_HOOK(HOOK_INIT, host_command_install_cb, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, host_command_init, HOOK_PRIO_DEFAULT);
#endif

#ifdef CONFIG_EC_HOST_CMD
static enum ec_status
host_command_get_cmd_versions(struct host_cmd_handler_args *args)
{
	const struct ec_host_cmd_handler *found_handler = NULL;
	const struct ec_params_get_cmd_versions *p = args->params;
	const struct ec_params_get_cmd_versions_v1 *p_v1 = args->params;
	struct ec_response_get_cmd_versions *r = args->response;

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

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CMD_VERSIONS, host_command_get_cmd_versions,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
#endif
