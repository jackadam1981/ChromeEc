/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if !defined(__CROS_EC_HOST_COMMAND_H) || \
	defined(__CROS_EC_ZEPHYR_HOST_COMMAND_H)
#error "This file must only be included from host_command.h. " \
	"Include host_command.h directly"
#endif
#define __CROS_EC_ZEPHYR_HOST_COMMAND_H

#include <stdbool.h>
#include <string.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>

/* Initializes and runs the host command handler loop.  */
void host_command_task(void *u);

/* Takes over the main thread and runs the host command loop. */
void host_command_main(void);

/*
 * Returns the main thread id. Will be the same as the HOSTCMD thread
 * when CONFIG_TASK_HOSTCMD_THREAD_MAIN is enabled.
 */
k_tid_t get_main_thread(void);

/*
 * Returns the HOSTCMD thread id. Will be different than the main thread
 * when CONFIG_TASK_HOSTCMD_THREAD_DEDICATED is enabled.
 */
k_tid_t get_hostcmd_thread(void);

#ifdef CONFIG_PLATFORM_EC_HOSTCMD

/**
 * See include/host_command.h for documentation.
 */
#ifdef CONFIG_EC_HOST_CMD

#include <zephyr/mgmt/ec_host_cmd/ec_host_cmd.h>

#ifndef CONFIG_ZTEST

#define DECLARE_HOST_COMMAND(id, handler, ver) \
	EC_HOST_CMD_HANDLER_UNBOUND(id, (ec_host_cmd_handler_cb)handler, ver)

#else

/*
 * Get a pointer to the response buffer of the host command called.
 */
void *host_command_get_response_buf(void);

/* Define a stub DECLARE_HOST_COMMAND macro for the tests with the upstream
 * Host Commands. It is required to pass the exact response buffer, get by
 * the host_command_process, to the command handler.
 */
#define DECLARE_HOST_COMMAND(id, handler, ver)				\
	static enum ec_status handler##_test(				\
		struct host_cmd_handler_args *args)			\
	{								\
		void *response = args->response;			\
		enum ec_status status;					\
									\
		args->response = host_command_get_response_buf();	\
		memcpy(args->response, response, args->response_max);	\
		status = handler(args);					\
		memcpy(response, args->response, args->response_max);	\
		args->response = response;				\
									\
		return status;						\
	}								\
	EC_HOST_CMD_HANDLER_UNBOUND(					\
		id, (ec_host_cmd_handler_cb)handler##_test, ver)

#endif /* CONFIG_ZTEST */

#else

#define DECLARE_HOST_COMMAND(_command, _routine, _version_mask)         \
	static const STRUCT_SECTION_ITERABLE(host_command,              \
					     _cros_hcmd_##_command) = { \
		.command = _command,                                    \
		.handler = _routine,                                    \
		.version_mask = _version_mask,                          \
	}

#endif /* CONFIG_EC_HOST_CMD */

#else /* !CONFIG_PLATFORM_EC_HOSTCMD */

/*
 * Create a global var to reference the host command. The linker should remove
 * it since it is never referenced.
 */
#define DECLARE_HOST_COMMAND(command, routine, version_mask) \
	int __remove_##command = ((int)(routine))

#endif /* CONFIG_PLATFORM_EC_HOSTCMD */
