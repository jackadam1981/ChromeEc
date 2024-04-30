/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 */

#include "drivers/intel_altmode.h"
#include "drivers/pdc.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc/pdc_power_mgmt.h"
#include "usbc/utils.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(usbpd_altmode, CONFIG_USB_PD_ALTMODE_LOG_LEVEL);

static enum ec_status hc_usb_pd_mux_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_pd_mux_info *p = args->params;
	struct ec_response_usb_pd_mux_info *r = args->response;
	int port = p->port;
	mux_state_t mux_state = 0;
	union connector_status_t connector_status = {0};

	if (port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	connector_status.conn_partner_flags = 0;
	pdc_power_mgmt_get_connector_status(port, &connector_status);

	/* TODO: Populate below info from UCSI commands */
	SET_MUX_STATE_IF(false, &mux_state, USB_PD_MUX_HPD_LVL);

	/* check orientation */
	SET_MUX_STATE_IF(connector_status.orientation, &mux_state,
			 USB_PD_MUX_POLARITY_INVERTED);

	/* USB mode */
	SET_MUX_STATE_IF(connector_status.conn_partner_flags & BIT(0),
			 &mux_state, USB_PD_MUX_USB_ENABLED);

	/* ALT Mode */
	if (connector_status.conn_partner_flags & BIT(1)) {
		if (connector_status.conn_partner_flags & BIT(2))
			SET_MUX_STATE_IF(true, &mux_state,
					 USB_PD_MUX_TBT_COMPAT_ENABLED);
		else if (connector_status.conn_partner_flags & BIT(3))
			SET_MUX_STATE_IF(true, &mux_state,
					 USB_PD_MUX_USB4_ENABLED);
		else
			SET_MUX_STATE_IF(true, &mux_state,
					 USB_PD_MUX_DP_ENABLED);
	}
	r->flags = mux_state;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_MUX_INFO, hc_usb_pd_mux_info,
		     EC_VER_MASK(0));

static int command_typec(const struct shell *sh, int argc, const char **argv)
{
	char *e;
	int port;
	union connector_status_t connector_status = {0};

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[1], &e, 10);
	if (*e || port >= board_get_usb_pd_port_count())
		return EC_ERROR_PARAM1;

	pdc_power_mgmt_get_connector_status(port, &connector_status);
	shell_fprintf(sh, SHELL_INFO,
		      "Port %d: USB=%d ALT_MODE=%d POLARITY=%s "
		      "HPD_LVL=%d TBT=%d USB4=%d\n",
		      port, !!(connector_status.conn_partner_flags & BIT(0)),
		      !!(connector_status.conn_partner_flags & BIT(1)),
		      connector_status.orientation ? "INVERTED" : "NORMAL",
		      -1, /* TODO */
		      !!(connector_status.conn_partner_flags & BIT(2)),
		      !!(connector_status.conn_partner_flags & BIT(3)));

	return EC_SUCCESS;
}
SHELL_CMD_REGISTER(typec, NULL, "gets typec port status.Usage:typec <port>",
		   command_typec);
