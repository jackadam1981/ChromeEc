/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if !defined(__CROS_EC_HOST_COMMAND_H) || \
	defined(__CROS_EC_ZEPHYR_HOST_COMMAND_H)
#error "This file must only be included from host_command.h. " \
	"Include host_command.h directly"
#endif
#define __CROS_EC_ZEPHYR_HOST_COMMAND_H

#include <zephyr/init.h>
#include <stdbool.h>

/* Initializes and runs the host command handler loop.  */
void host_command_task(void *u);

/* Takes over the main thread and runs the host command loop. */
void host_command_main(void);

/* True if running in the main thread. */
bool in_host_command_main(void);

#ifdef CONFIG_PLATFORM_EC_HOSTCMD
// #if 1

// struct host_cmd_handler_args {
// 	void (*send_response)(struct host_cmd_handler_args *args);
// 	uint16_t command;      /* Command (e.g., EC_CMD_FLASH_GET_INFO) */
// 	uint8_t version;       /* Version of command (0-31) */
// 	const void *params; /* Input parameters */
// 	uint16_t params_size;  /* Size of input parameters in bytes */
// 	void *response;
// 	uint16_t response_max;
// 	uint16_t response_size;
// 	uint16_t result;
// };

static inline void from_zephyr_to_ec(struct ec_host_cmd_handler_args *zeph, struct host_cmd_handler_args *ec)
{
	ec->version = zeph->version;
	ec->params = zeph->input_buf;
	ec->params_size = zeph->input_buf_size;
	ec->response = zeph->output_buf;
	printk("(HC) Input buf: %p, Output buf: %p\n", zeph->input_buf, zeph->output_buf);
	ec->response_max = ec->response_size = zeph->output_buf_size;
}

static inline void from_ec_to_zephyr(struct host_cmd_handler_args *ec, struct ec_host_cmd_handler_args *zeph)
{
	// zeph->version = ec->version;
	// zeph->input_buf = ec->params;
	// zeph->input_buf_size = ec->params_size;
	// zeph->output_buf = ec->response;
	zeph->output_buf_size = ec->response_size;
}

// static int yolo_handler(struct ec_host_cmd_handler_args *args)
// {
// 	struct host_cmd_handler_args ec_args;
// 	int res;

// 	from_zephyr_to_ec(args, &ec_args);
// 	res = handler(&ec_args);
// 	from_ec_to_zephyr(&ec_args, args);
// 	return res;
// }

/**
 * See include/host_command.h for documentation.
 */
// #define DECLARE_HOST_COMMAND(_command, _routine, _version_mask)
// 	STRUCT_SECTION_ITERABLE(host_command, _cros_hcmd_##_command) = {
// 		.command = _command,
// 		.handler = _routine,
// 		.version_mask = _version_mask,
// 	}
#include<zephyr/mgmt/ec_host_cmd.h>
#define DECLARE_HOST_COMMAND(id, handler, ver) \
static enum ec_host_cmd_status shim_##handler(struct ec_host_cmd_handler_args *args) \
{ \
	struct host_cmd_handler_args ec_args; \
	int res; \
	from_zephyr_to_ec(args, &ec_args); \
	res = handler(&ec_args); \
	from_ec_to_zephyr(&ec_args, args); \
	return (enum ec_host_cmd_status)res; \
} \
EC_HOST_CMD_HANDLER_UNBOUND(shim_##handler, id, ver)
//#define EC_HOST_CMD_HANDLER(...)
#else /* !CONFIG_PLATFORM_EC_HOSTCMD */

/*
 * Create a fake routine to call the function. The linker should
 * garbage-collect it since it is behind 'if (0)'
 */
#define DECLARE_HOST_COMMAND(command, routine, version_mask)		\
	int __remove_ ## command(void)					\
	{								\
		if (0)							\
			routine(NULL);					\
		return 0;						\
	}

#endif /* CONFIG_PLATFORM_EC_HOSTCMD */
