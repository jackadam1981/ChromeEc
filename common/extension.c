/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "byteorder.h"
#include "console.h"
#include "extension.h"
#include "link_defs.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

void extension_route_command(uint16_t command_code,
			    void *buffer,
			    size_t in_size,
			    size_t *out_size)
{
	struct extension_command *cmd_p;
	struct extension_command *end_p;

	cmd_p = (struct extension_command *)&__extension_cmds;
	end_p = (struct extension_command *)&__extension_cmds_end;

	while (cmd_p != end_p) {
		if (cmd_p->command_code == command_code) {
			cmd_p->handler(buffer, in_size, out_size);
			return;
		}
		cmd_p++;
	}

	CPRINTF("%s: handler %d not found\n", __func__, command_code);

	/* This covers the case of the handler not found. */
	*out_size = 0;
}

void call_extension_command(void *buffer, size_t *total_size)
{
	struct tpm_cmd_header *tpmh = buffer;
	size_t command_size = be32toh(tpmh->size);

	/* Verify there is room for at least the extension command header. */
	if (command_size >= sizeof(struct tpm_cmd_header)) {
		uint16_t subcommand_code;

		/* The header takes room in the buffer. */
		*total_size -= sizeof(struct tpm_cmd_header);

		subcommand_code = be16toh(tpmh->subcommand_code);
		extension_route_command(subcommand_code,
				       tpmh + 1,
				       command_size -
				       sizeof(struct tpm_cmd_header),
				       total_size);
		/* Add the header size back. */
		*total_size += sizeof(struct tpm_cmd_header);
		tpmh->size = htobe32(*total_size);
	} else {
		*total_size = command_size;
	}
}
