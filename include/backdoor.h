/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_INCLUDE_BACKDOOR_H
#define __EC_INCLUDE_BACKDOOR_H

#include "tpm_registers.h"
#include "util.h"

typedef void (*backdoor_handler)(const void *command_body,
				 size_t command_size,
				 void *command_response,
				 size_t *response_size);  /* Both in and out. */

void backdoor_route_command(struct tpm_cmd_header *tpmh,
			    unsigned *out_size,
			    uint8_t **out_buffer);

/* Zero return value means failure. */
int backdoor_register_handler(uint16_t subcommand_code,
			      backdoor_handler handler);
int backdoor_unregister_handler(uint16_t subcommand_code);

#endif  /* __EC_INCLUDE_BACKDOOR_H */
