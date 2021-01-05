/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault host and console commands
 */


#include "host_command.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/data_packetizer.h"
#include "memfault_demo_shell_commands.h"
#include <string.h>
#include <console.h>

static enum ec_status memfault_hostcmd_get_chunk(struct host_cmd_handler_args *args)
{
	bool chunk_available;
	size_t buf_len = args->response_max;

	chunk_available = memfault_packetizer_get_chunk(args->response, &buf_len);
	if(!chunk_available) {
		args->response_size = 0;
		return EC_RES_SUCCESS;
	}

	args->response_size = buf_len;

	MEMFAULT_LOG_INFO("memfault_hostcmd_get_chunk: %d byte chunk returned", buf_len);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_MEMFAULT_GET_CHUNK,
			memfault_hostcmd_get_chunk,
			EC_VER_MASK(0));


/***************************************
 * Memfault demo cli
 *
 */

#define MEMFAULT_SHELL_FOR_EACH_COMMAND(command) \
	for (const sMemfaultShellCommand *command = g_memfault_shell_commands; \
		command < &g_memfault_shell_commands[g_memfault_num_shell_commands]; \
		++command)


static const sMemfaultShellCommand *prv_find_command(const char *name) {
	MEMFAULT_SHELL_FOR_EACH_COMMAND(command) {
		if (strncmp(command->command, name, 64) == 0) {
			return command;
		}
	}
	return NULL;
}

static int memfault_console_command(int argc, char **argv)
{
	if (argc >= 2) {
		const sMemfaultShellCommand *command = prv_find_command(argv[1]);
		if (!command) {
			ccprintf("Unknown command: %s\n", argv[1]);
		} else {
			command->handler(argc-1, &(argv[1]));
		}
	} else {
		ccprintf("Not enough args\n");
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(memfault, memfault_console_command,
			"cmd",
			"Memfault CLI");
