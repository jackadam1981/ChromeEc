/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB-C Power Path Controller Common Code */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "timer.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

static uint8_t oc_event_cnt_tbl[CONFIG_USB_PD_PORT_COUNT];
static uint8_t clear_oc_event_att[CONFIG_USB_PD_PORT_COUNT];

/* Simple wrappers to dispatch to the drivers. */

int ppc_init(int port)
{
	int rv;

	if (port >= ppc_cnt)
		return EC_ERROR_INVAL;

	rv = ppc_chips[port].drv->init(port);
	if (rv)
		CPRINTS("p%d: PPC init failed! (%d)", port, rv);
	else
		CPRINTS("p%d: PPC init'd.", port);

	return rv;
}

int ppc_add_oc_event(int port)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	oc_event_cnt_tbl[port]++;

	if (oc_event_cnt_tbl[port] >= PPC_OC_CNT_THRESH)
		CPRINTS("C%d: OC event limit reached!  Source path disabled.",
			port);
	return EC_SUCCESS;
}

/* Flag to prevent perpetually postponing the clearing attempts table. */
static int clear_oc_tbl_scheduled;

static void clear_oc_tbl(void)
{
	int i;

	for (i = 0; i < ppc_cnt; i++)
		if (clear_oc_event_att[i] > PPC_OC_CNT_THRESH)
			clear_oc_event_att[i] = 0;

	clear_oc_tbl_scheduled = 0;
}
DECLARE_DEFERRED(clear_oc_tbl);

int ppc_clear_oc_event_counter(int port)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	clear_oc_event_att[port]++;

	/*
	 * If we are clearing our event table in quick succession, we may be in
	 * an overcurrent loop where we are also detecting a disconnect on the
	 * CC pins.  Therefore, let's not clear it just yet and the let the
	 * limit be reached.  This way, we won't send the hard reset and
	 * actually detect the physical disconnect.
	 */
	if (clear_oc_event_att[port] > PPC_OC_CNT_THRESH) {
		if (!clear_oc_tbl_scheduled) {
			hook_call_deferred(&clear_oc_tbl_data, 2 * SECOND);
			clear_oc_tbl_scheduled = 1;
		}

		return EC_ERROR_ACCESS_DENIED;
	}

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

#ifdef CONFIG_USBC_PPC_POLARITY
int ppc_set_polarity(int port, int polarity)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	return ppc_chips[port].drv->set_polarity(port, polarity);
}
#endif

int ppc_set_vbus_source_current_limit(int port, enum tcpc_rp_value rp)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	return ppc_chips[port].drv->set_vbus_source_current_limit(port, rp);
}

int ppc_discharge_vbus(int port, int enable)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	return ppc_chips[port].drv->discharge_vbus(port, enable);
}

int ppc_is_port_latched_off(int port)
{
	if ((port < 0) || (port >= ppc_cnt))
		return 0;

	return oc_event_cnt_tbl[port] >= PPC_OC_CNT_THRESH;
}

#ifdef CONFIG_USBC_PPC_SBU
int ppc_set_sbu(int port, int enable)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	return ppc_chips[port].drv->set_sbu(port, enable);
}
#endif /* defined(CONFIG_USBC_PPC_SBU) */

#ifdef CONFIG_USBC_PPC_VCONN
int ppc_set_vconn(int port, int enable)
{
	if ((port < 0) || (port >= ppc_cnt))
		return EC_ERROR_INVAL;

	/*
	 * Check our OC event counter.  If we've exceeded our threshold, then
	 * let's latch our source path off to prevent continuous cycling.  When
	 * the PD state machine detects a disconnection on the CC lines, we will
	 * reset our OC event counter.
	 */
	if (enable && (oc_event_cnt_tbl[port] >= PPC_OC_CNT_THRESH))
		return EC_ERROR_ACCESS_DENIED;

	return ppc_chips[port].drv->set_vconn(port, enable);
}
#endif

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
	if (enable && (oc_event_cnt_tbl[port] >= PPC_OC_CNT_THRESH))
		return EC_ERROR_ACCESS_DENIED;

	return ppc_chips[port].drv->vbus_source_enable(port, enable);
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
int ppc_is_vbus_present(int port)
{
	if ((port < 0) || (port >= ppc_cnt)) {
		CPRINTS("%s(%d) Invalid port!", __func__, port);
		return 0;
	}

	return ppc_chips[port].drv->is_vbus_present(port);
}
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */

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
