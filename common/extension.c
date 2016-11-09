/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <endian.h>

#include "console.h"
#include "extension.h"
#include "link_defs.h"
#include "tpm_registers.h"

#define CPRINTF(format, args...) cprintf(CC_TPM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_TPM, format, ## args)

void extension_route_command(uint16_t command_code,
			    void *buffer,
			    size_t in_size,
			    size_t *out_size)
{
	const struct extension_command *cmd_p;

	for (cmd_p = __extension_cmds; cmd_p != __extension_cmds_end; cmd_p++) {
		if (cmd_p->command_code == command_code) {
			cmd_p->handler.extension(buffer, in_size, out_size);
			return;
		}
	}

	CPRINTF("%s: handler %d not found\n", __func__, command_code);

	/* This covers the case of the handler not found. */
	*out_size = 0;
}

static void call_with_extension_protocol(struct tpm_cmd_header *tpmh,
					 size_t *total_size)
{
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

static enum vendor_cmd_rc route_vendor_cmd(enum vendor_cmd_cc command_code,
					   uint8_t *buffer,
					   uint32_t input_size,
					   uint32_t *resp_size)
{
	const struct extension_command *cmd_p;

	CPRINTS("%s: cc is %d, input_size %d, resp_size %d", __func__,
		command_code, input_size, *resp_size);

	for (cmd_p = __extension_cmds; cmd_p != __extension_cmds_end; cmd_p++)
		if (cmd_p->command_code == command_code)
			return cmd_p->handler.vendor(command_code, buffer,
						     input_size, resp_size);

	CPRINTF("%s: handler %d not found\n", __func__, command_code);

	*resp_size = 0;
	return VENDOR_RC_NO_SUCH_COMMAND;
}

static void call_with_vendor_protocol(uint32_t command_code,
				      void *inout, uint32_t *inout_size)
{
	struct tpm_common_header *tpmh = inout;
	enum vendor_cmd_cc cc = command_code & VENDOR_CC_MASK;
	uint32_t input_size = be32toh(tpmh->size);
	uint32_t resp_size, resp_code;
	enum vendor_cmd_rc rc;

	CPRINTS("%s: cc %d, input_size %d, inout_size %d", __func__,
		cc, input_size, *inout_size);

	if (input_size < sizeof(*tpmh) || input_size > *inout_size ||
	    cc <= LAST_EXTENSION_COMMAND || cc > LAST_VENDOR_COMMAND) {
		rc = VENDOR_RC_BOGUS_ARGS;
		resp_size = 0;
	} else {
		resp_size = *inout_size - sizeof(*tpmh);
		rc = route_vendor_cmd(cc,
				      (uint8_t *)(tpmh + 1),
				      input_size - sizeof(*tpmh),
				      &resp_size);
	}

	CPRINTS("%s: rc %d, resp_size %d", __func__, rc, resp_size);

	*inout_size = sizeof(*tpmh) + resp_size;
	resp_code = rc ? VENDOR_RC_ERR + rc : rc;

	tpmh->size = htobe32(*inout_size);
	tpmh->code = htobe32(resp_code);

	CPRINTS("%s: code 0x%x, size 0x%x", __func__, resp_code, *inout_size);
}

void call_extension_command(uint32_t command_code,
			    void *buffer, size_t *total_size)
{
	if (command_code == CONFIG_EXTENSION_COMMAND)
		call_with_extension_protocol(buffer, total_size);
	else
		call_with_vendor_protocol(command_code, buffer, total_size);
}
