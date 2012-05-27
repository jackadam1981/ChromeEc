/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Host command module for Chrome EC */

#ifndef __CROS_EC_HOST_COMMAND_H
#define __CROS_EC_HOST_COMMAND_H

#include "common.h"
#include "ec_commands.h"

/* Host command */
struct host_command {
	/* Command code. */
	int command;
	/* Handler for the command; data points to parameters/response.
	 * returns negative error code if case of failure (using EC_LPC_STATUS
	 * codes). sets <response_size> if it returns a payload to the host. */
	int (*handler)(uint8_t *data, int *response_size);
};

/**
 * Process a host command and return its response
 *
 * @param slot		is 0 for kernel-originated commands,
 *			1 for usermode-originated commands.
 * @param command	The command code
 * @param data		Buffer holding the command parameters, and used for
 *			the response payload.
 * @param response_size	Returns the size of the response
 * @return resulting status
 */
enum ec_status host_command_process(int slot, int command, uint8_t *data,
				    int *response_size);

/**
 * Queue up a host command to process later
 *
 * When we do get around to processing it, we will use the buffer to put
 * the response.
 *
 * @param slot		is 0 for kernel-originated commands,
 *			1 for usermode-originated commands.
 * @param command	The command code
 * @param buffer	Buffer holding the command parameters, and used for
 *			the response payload.
 * @param size		Size of the command parameters, in bytes (-1=unknown)
 * @param maxsize	Maximum size of buffer (used for response)
 */
void host_command_received(int slot, int command, uint8_t *buffer,
			   int size, int maxsize);

   // success results with response data
/* Send a successful result code along with response data to a host command.
 * <slot> is 0 for kernel-originated commands,
 *           1 for usermode-originated commands.
 * <result> is the result code for the command (EC_RES_...)
 * <data> is the buffer with the response payload.
 * <size> is the size of the response buffer. */
void host_send_response(int slot, enum ec_status result, const uint8_t *data,
			int size);

/* Register a host command handler */
#define DECLARE_HOST_COMMAND(command, routine)				\
	const struct host_command __host_cmd_##command			\
	__attribute__((section(".rodata.hcmds")))			\
	     = {command, routine}

#endif  /* __CROS_EC_HOST_COMMAND_H */
