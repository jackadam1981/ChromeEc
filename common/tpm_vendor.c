/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <endian.h>

#include "common.h"
#include "console.h"
#include "link_defs.h"
#include "tpm_registers.h"
#include "tpm_vendor.h"

#define CPRINTF(format, args...) cprintf(CC_TPM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_TPM, format, ## args)


static enum vendor_cmd_rc route_vendor_cmd(enum vendor_cmd_cc command_code,
					   uint8_t *buffer,
					   uint32_t input_size,
					   uint32_t *resp_size)
{
	const struct vendor_cmd_s *cmd_p;

	CPRINTS("%s: cc is %d, input_size %d, resp_size %d", __func__,
		command_code, input_size, *resp_size);

	for (cmd_p = __vendor_cmds; cmd_p != __vendor_cmds_end; cmd_p++)
		if (cmd_p->command_code == command_code)
			return cmd_p->handler(command_code, buffer,
					      input_size, resp_size);

	CPRINTF("%s: handler %d not found\n", __func__, command_code);

	*resp_size = 0;
	return VENDOR_RC_NO_SUCH_COMMAND;
}

void call_vendor_cmd(void *inout, uint32_t *inout_size)
{
	struct tpm_common_header *tpmh = inout;
	enum vendor_cmd_cc cc = be32toh(tpmh->code) & VENDOR_CC_MASK;
	uint32_t input_size = be32toh(tpmh->size);
	uint32_t resp_size, resp_code;
	enum vendor_cmd_rc rc;

	CPRINTS("%s: cc %d, input_size %d, inout_size %d", __func__,
		cc, input_size, *inout_size);

	if (input_size < sizeof(*tpmh) || input_size > *inout_size) {
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
