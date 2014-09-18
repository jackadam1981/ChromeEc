/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "atomic.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "host_command.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "version.h"

extern struct svdm_response svdm_rsp;

#ifdef CONFIG_USB_PD_ALT_MODE

int pd_svdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload)
{
	int cmd = PD_VDO_CMD(payload[0]);
	int cmd_type = PD_VDO_CMDT(payload[0]);

	int rsize = 1; /* VDM header at a minimum */
	ccprintf("%T] SVDM/%d [%d] %08x\n", cnt, cmd, payload[0]);
	*rpayload = payload;

	if (cmd_type == CMDT_INIT) {
		switch (cmd) { 
		case VDO_CMD_DISCOVER_IDENT:
			rsize = svdm_rsp.identity(payload);
			break;
		}
		payload[0] |= VDO_CMDT(CMDT_RSP_ACK);
	} else if (cmd_type == CMDT_RSP_ACK) {
		switch (cmd) { 
		case VDO_CMD_DISCOVER_IDENT:
			/* TODO(tbroch) store this in somewhere */
			ccprintf("IDENTITY: hdr:%08x Cert Stat VDO:%08x "
				 "Cable VDO:%08x\n",
				 payload[1], payload[2], payload[3]);
		break;
		}
		rsize = 0;
	}
	ccprintf("%T] DONE\n");
	return rsize;
}
#else

int pd_svdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload)
{
	return 0;
}
#endif /* CONFIG_USB_PD_ALT_MODE */

#ifndef CONFIG_USB_PD_CUSTOM_VDM
int pd_custom_vdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload)
{
	return 0;
}
#endif /* !CONFIG_USB_PD_CUSTOM_VDM */
