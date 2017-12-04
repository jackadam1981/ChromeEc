/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB-C Power Path Controller Common Code */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static uint8_t oc_event_cnt_tbl[CONFIG_USB_PD_PORT_COUNT];

/* Simple wrappers to dispatch to the drivers. */

int ppc_add_oc_event(int port)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	oc_event_cnt_tbl[port]++;
	return EC_SUCCESS;
}

int ppc_clear_oc_event_counter(int port)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	oc_event_cnt_tbl[port] = 0;
	return EC_SUCCESS;
}

int ppc_is_sourcing_vbus(int port)
{
	if ((port < 0) || (port >= ppc_cnt)) {
		CPRINTS("%s(%d) Invalid port!", __func__, port);
		return 0;
	}

	return ppc_chips[port].drv->is_sourcing_vbus(port);
}

int ppc_vbus_sink_enable(int port, int enable)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	return ppc_chips[port].drv->vbus_sink_enable(port, enable);
}

int ppc_vbus_source_enable(int port, int enable)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	/*
	 * Check our OC event counter.  If we've exceeded our threshold, then
	 * let's latch our source path off to prevent continuous cycling.  When
	 * the PD state machine detects a disconnection on the CC lines, we will
	 * reset our OC event counter.
	 */
	if (enable && (oc_event_cnt_tbl[port] >= PPC_OC_CNT_THRESH)) {
		CPRINTS("p%d: OC event limit reached!  Source path disabled.",
			port);
		return EC_ERROR_ACCESS_DENIED;
	}

	return ppc_chips[port].drv->vbus_source_enable(port, enable);
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
int ppc_is_vbus_present(int port, int *vbus_present)
{
	if (port >= ppc_cnt)
		return EC_ERROR_INVAL;

	if (ppc_chips[port].drv->get_vbus_status == NULL)
		return EC_ERROR_UNIMPLEMENTED;

	return ppc_chips[port].drv->get_vbus_status(port, vbus_present);
}
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */

static void ppc_init(void)
{
	int i;
	int rv;

	for (i = 0; i < ppc_cnt; i++) {
		oc_event_cnt_tbl[i] = 0;
		rv = ppc_chips[i].drv->init(i);
		if (rv)
			CPRINTS("p%d: PPC init failed! (%d)", i, rv);
		else
			CPRINTS("p%d: PPC init'd.", i);
	}
}
DECLARE_HOOK(HOOK_INIT, ppc_init, HOOK_PRIO_INIT_I2C + 1);

#ifdef CONFIG_CMD_PPC_DUMP
static int command_ppc_dump(int argc, char **argv)
{
	int port;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	port = atoi(argv[1]);
	if (port >= ppc_cnt)
		return EC_ERROR_PARAM1;

	return ppc_chips[port].drv->reg_dump(port);
}
DECLARE_CONSOLE_COMMAND(ppc_dump, command_ppc_dump, "<Type-C port>",
			"dump the PPC regs");
#endif /* defined(CONFIG_CMD_PPC_DUMP) */
