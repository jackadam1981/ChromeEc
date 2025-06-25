/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "console.h"
#include "extension.h"
#include "link_defs.h"
#include "strongbox.h"


#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ##args)

uint32_t extension_route_strongbox_command(struct vendor_cmd_params *p)
{
	const struct strongbox_command *cmd_p;
	const struct strongbox_command *end_p;

#ifdef DEBUG_EXTENSION
	CPRINTS("%s(%d,%s) is=%d os=%d", __func__, p->code,
		p->flags & VENDOR_CMD_FROM_USB ? "USB" : "AP", p->in_size,
		p->out_size);
#endif
	/* Check that command came from valid interface in a valid state. */
	if ((p->flags & (VENDOR_CMD_FROM_USB | VENDOR_CMD_FROM_ALT_IF))
#ifdef CONFIG_BOARD_ID_SUPPORT
	    || board_id_is_mismatched()
#endif
	)
		return SBERR_HardwareNotYetAvailable;

	/* Find the command handler */
	cmd_p = (const struct strongbox_command *)&__strongbox_cmds;
	end_p = (const struct strongbox_command *)&__strongbox_cmds_end;
	while (cmd_p != end_p) {
		if (cmd_p->command_code == p->code)
			return cmd_p->handler(p);
		cmd_p++;
	}

	/* Command not found or not allowed */
	p->out_size = 0;
	return SBERR_Unimplemented;
}

enum strongbox_error sb_GetHardwareInfo(struct vendor_cmd_params *p)
{
	static const uint8_t r[35] = { /* version */
				     0x00, 0x00, 0x00, 0x00,
				     /* SecurityLevel Strongbox */
				     0x00, 0x00, 0x00, 0x02,
				     /* Keymint name */
				     0x00, 0x04, 'C', 'R', '5', '0',
				     /* Key mint author */
				     0x00, 0x06, 'G', 'O', 'O', 'G', 'L', 'E',
				     /* timestamp_token_required = false */
				     0x00
	};

	p->out_size = 0;
	if (p->in_size)
		return SBERR_InvalidArgument;

	p->out_size = sizeof(r);
	memcpy(p->buffer, &r, sizeof(r));
	return SB_OK;
}
DECLARE_STRONGBOX_COMMAND(SB_DeviceGetHardwareInfo, sb_GetHardwareInfo);
