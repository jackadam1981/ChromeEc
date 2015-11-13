/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_INCLUDE_BACKDOOR_H
#define __EC_INCLUDE_BACKDOOR_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"

/*
 * Type of function handling backdoor commands.
 *
 * @param buffer        As input points to the input data to be processed, as
 *                      output stores data, processing result.
 * @param command_size  Number of bytes of input data
 * @param response_size On input - max size of the buffer, on output - actual
 *                      number of data returned by the handler.
 */
typedef void (*backdoor_handler)(void *buffer,
				 size_t command_size,
				 size_t *response_size);
/*
 * Find handler for a backdoor command.
 *
 * @param command_code Code associated with a backdoor command handler.
 * @param buffer       Data to be processd by the handler, the same space
 *                     is used for data returned by the handler.
 * @command_size       Size of the input data.
 * @param size         On input - max size of the buffer, on output - actual
 *                     number of data returned by the handler.
 */
void backdoor_route_command(uint16_t command_code,
			    void *buffer,
			    size_t command_size,
			    size_t *size);

struct backdoor_command {
	uint16_t command_code;
	backdoor_handler handler;
} __packed;

/* Values for different backdoor commands. */
enum {
	BACKDOOR_AES = 0,
};

#define DECLARE_BACKDOOR_COMMAND(code, handler) \
	const struct backdoor_command __keep __backdoor_cmd_##code \
	__attribute__((section(".rodata.backdoorcmds")))	   \
	= {code, handler}

#endif  /* __EC_INCLUDE_BACKDOOR_H */
