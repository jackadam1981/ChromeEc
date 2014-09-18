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

/* TODO(tbroch) is there a finite number for these in the spec */
#define SVID_DISCOVERY_MAX 16

#ifndef PD_PORT_COUNT
#define PD_PORT_COUNT 1
#endif

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static struct pd_policy {
	/* count svids discovered */
	int svid_cnt;
	/* index of svid currently being operated on */
	int svid_idx;
	/* index of mode data being entered/exited.  Note its n - 1 of object
	 * position field in VDM header. */
	int mode_idx;
	struct svdm_svid_data svids[SVID_DISCOVERY_MAX];
} pe[PD_PORT_COUNT];

static void pe_init(int port)
{
	pe[port].svid_cnt = 0;
	pe[port].svid_idx = 0;
	pe[port].mode_idx = 0;
	memset(pe[port].svids, 0,
	       sizeof(struct svdm_svid_data) * SVID_DISCOVERY_MAX);
}

static void dfp_consume_identity(int port, uint32_t *payload)
{
	int ptype = PD_IDH_PTYPE(payload[VDO_I(IDH)]);
	pe_init(port);
	switch (ptype) {
	case IDH_PTYPE_AMA:
		/* TODO(tbroch) do I disable VBUS here if power contract
		 * requested it
		 */
		if (!PD_VDO_AMA_VBUS_REQ(payload[VDO_I(AMA)]))
			pd_power_supply_reset(port);
		break;
		/* TODO(crosbug.com/p/30645) provide vconn support here */
	default:
		break;
	}
}

static int dfp_discover_svids(int port, uint32_t *payload)
{
	payload[0] = VDO(USB_SID_PD, 1, CMD_DISCOVER_SVID);
	return 1;
}

static void dfp_consume_svids(int port, uint32_t *payload)
{
	int i;
	uint32_t *ptr = payload + 1;
	uint16_t svid0, svid1;

	for (i = pe[port].svid_cnt; i < pe[port].svid_cnt + 12; i += 2) {
		if (i == SVID_DISCOVERY_MAX) {
			ccprintf("ERR: too many svids discovered\n");
			break;
		}

		svid0 = PD_VDO_SVID_SVID0(*ptr);
		if (!svid0)
			break;
		pe[port].svids[i].svid = svid0;
		pe[port].svid_cnt++;

		svid1 = PD_VDO_SVID_SVID1(*ptr);
		if (!svid1)
			break;
		pe[port].svids[i + 1].svid = svid1;
		pe[port].svid_cnt++;
		ptr++;
	}
	/* TODO(tbroch) need to re-issue discover svids if > 12 */
	if (i && ((i % 12) == 0))
		ccprintf("TODO: need to re-issue discover svids > 12\n");
}

static int dfp_discover_modes(int port, uint32_t *payload)
{
	uint16_t svid = pe[port].svids[pe[port].svid_idx].svid;
	payload[0] = VDO(svid, 1, CMD_DISCOVER_MODES);
	return 1;
}

static int dfp_consume_modes(int port, uint32_t *payload)
{
	memcpy(pe[port].svids[pe[port].svid_idx].mode_vdo, &payload[1],
	       sizeof(uint32_t) * PDO_MODES);
	pe[port].svid_idx++;
	return (pe[port].svid_idx < pe[port].svid_cnt);
}

/* TODO(tbroch) standard allows to enter multiple modes.  Implement that. */
static struct svdm_mode_data *dfp_mode;

static int dfp_enter_mode(int port, uint32_t *payload)
{
	uint32_t **mode_vdo = NULL;

	dfp_mode = pd_dfp_choose_mode(pe[port].svid_cnt, pe[port].svids,
				      mode_vdo, &pe[port].mode_idx);
	if (!dfp_mode) {
		ccprintf("PE: no mode chosen\n");
		return 0;
	}
	dfp_mode->enter(*mode_vdo[pe[port].mode_idx]);
	payload[0] = VDO(dfp_mode->svid, 1,
			 CMD_ENTER_MODE | VDO_OPOS((pe[port].mode_idx + 1)));
	return 1;
}

static int dfp_exit_mode(int port, uint32_t *payload)
{
	if (!dfp_mode) {
		ccprintf("PE ERR: no mode chosen\n");
		return 0;
	}
	dfp_mode->exit();
	payload[0] = VDO(dfp_mode->svid, 1,
			 CMD_EXIT_MODE | VDO_OPOS((pe[port].mode_idx + 1)));
	return 1;
}

static void dump_pe(int port)
{
	int i, j;

	for (i = 0; i < pe[port].svid_cnt; i++) {
		ccprintf("SVID: %04x", pe[port].svids[i].svid);
		for (j = 0; j < (PDO_MAX_OBJECTS - 1); j++)
			ccprintf(" [%d] %08x", j,
				 pe[port].svids[i].mode_vdo[j]);
		ccprintf("\n");
	}
}

static int command_pe(int argc, char **argv)
{
	int port;
	char *e;
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;
	/* command: pe <port> <subcmd> <args> */
	port = strtoi(argv[1], &e, 10);
	if (*e || port >= PD_PORT_COUNT)
		return EC_ERROR_PARAM2;
	if (!strncasecmp(argv[2], "dump", 4))
		dump_pe(port);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(pe, command_pe,
			"<port> dump",
			"USB PE",
			NULL);

#endif /* CONFIG_USB_PD_ALT_MODE_DFP */

int pd_svdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload)
{
	int i;
	int cmd = PD_VDO_CMD(payload[0]);
	int cmd_type = PD_VDO_CMDT(payload[0]);

	int rsize = 1; /* VDM header at a minimum */
	ccprintf("%T] SVDM/%d [%d] %08x", cnt, cmd, payload[0]);
	for (i = 1; i < cnt; i++)
		ccprintf(" %08x", payload[i]);
	ccprintf("\n");

	payload[0] &= ~VDO_CMDT_MASK;
	*rpayload = payload;

	if (cmd_type == CMDT_INIT) {
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
			rsize = svdm_rsp.identity(port, payload);
			break;
		case CMD_DISCOVER_SVID:
			rsize = svdm_rsp.svids(port, payload);
			break;
		case CMD_DISCOVER_MODES:
			rsize = svdm_rsp.modes(port, payload);
			break;
		case CMD_ENTER_MODE:
			rsize = svdm_rsp.enter_mode(port, payload);
			break;
		case CMD_EXIT_MODE:
			rsize = svdm_rsp.exit_mode(port, payload);
			break;
		}
		if (rsize > 1)
			payload[0] |= VDO_CMDT(CMDT_RSP_ACK);
		else if (rsize == 1)
			payload[0] |= VDO_CMDT(CMDT_RSP_NAK);
		else {
			payload[0] |= VDO_CMDT(CMDT_RSP_BUSY);
			rsize = 1;
		}
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	} else if (cmd_type == CMDT_RSP_ACK) {
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
			dfp_consume_identity(port, payload);
			rsize = dfp_discover_svids(port, payload);
			break;
		case CMD_DISCOVER_SVID:
			dfp_consume_svids(port, payload);
			rsize = dfp_discover_modes(port, payload);
			break;
		case CMD_DISCOVER_MODES:
			if (dfp_consume_modes(port, payload))
				rsize = dfp_discover_modes(port, payload);
			else
				rsize = dfp_enter_mode(port, payload);
			break;
		case CMD_ENTER_MODE:
			/* TODO(tbroch) when would we not enter mode directly
			   after discovery? */
			rsize = 0;
			break;
		case CMD_EXIT_MODE:
			rsize = dfp_exit_mode(port, payload);
			break;
		}
		payload[0] &= ~VDO_CMDT(0);
		payload[0] |= VDO_CMDT(CMDT_INIT);
	} else if (cmd_type == CMDT_RSP_BUSY) {
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
		case CMD_DISCOVER_SVID:
		case CMD_DISCOVER_MODES:
			/* resend if its discovery */
			payload[0] &= ~VDO_CMDT(0);
			payload[0] |= VDO_CMDT(CMDT_INIT);
			rsize = 1;
			break;
		case CMD_ENTER_MODE:
			/* Error */
			ccprintf("PE ERR: received BUSY for Enter mode\n");
			rsize = 0;
			break;
		case CMD_EXIT_MODE:
			rsize = 0;
			break;
		}
	} else if (cmd_type == CMDT_RSP_NAK) {
		/* nothing to do */
		rsize = 0;
#endif /* CONFIG_USB_PD_ALT_MODE_DFP */
	} else {
		ccprintf("PE ERR: unknown cmd type %d\n", cmd);
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
int pd_vdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload)
{
	return 0;
}
#endif /* !CONFIG_USB_PD_CUSTOM_VDM */
