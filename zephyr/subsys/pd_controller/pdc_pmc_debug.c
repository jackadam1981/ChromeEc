/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 */

#include "console.h"
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

static int cmd_get_pd_port(const struct shell *sh, char *arg_val, uint8_t *port)
{
	char *e;

	*port = strtoul(arg_val, &e, 0);
	if (*e || *port >= CONFIG_USB_PD_PORT_MAX_COUNT) {
		shell_error(sh, "Invalid port");
		return -EINVAL;
	}

	return 0;
}

static int cmd_altmode_read(const struct shell *sh, size_t argc, char **argv)
{
	int rv, i;
	uint8_t port;
	union data_status_reg status = { 0 };

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	/* Read from status register */
	rv = pdc_power_mgmt_get_pch_data_status(port, status.raw_value);
	if (rv) {
		shell_error(sh, "Read failed, rv=%d", rv);
		/* return rv; */
	}

	shell_fprintf(sh, SHELL_INFO, "DATA_STATUS (msb-lsb): ");
	for (i = INTEL_ALTMODE_DATA_STATUS_REG_LEN - 1; i >= 0; i--)
		shell_fprintf(sh, SHELL_INFO, "%02x ", status.raw_value[i]);

	shell_info(sh, "");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_altmode_cmds,
			       SHELL_CMD_ARG(read, NULL,
					     "Read status register\n"
					     "Usage: altmode read <port>",
					     cmd_altmode_read, 2, 1),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(altmode, &sub_altmode_cmds, "PD Altmode commands", NULL);

#ifdef CONFIG_CMD_TYPEC
static int pdc_pmc_alt_mode_get_mux(int port, mux_state_t *mux_state)
{
	union data_status_reg status;
	int rv;

	rv = pdc_power_mgmt_get_pch_data_status(port, status.raw_value);
	if (rv) {
		LOG_ERR("Read failed, rv=%d", rv);
		return rv;
	}

	SET_MUX_STATE_IF(status.conn_ori, mux_state,
			 USB_PD_MUX_POLARITY_INVERTED);
	SET_MUX_STATE_IF(status.usb2 || status.usb3_2, mux_state,
			 USB_PD_MUX_USB_ENABLED);
	SET_MUX_STATE_IF(status.dp, mux_state, USB_PD_MUX_DP_ENABLED);
	SET_MUX_STATE_IF(status.hpd_lvl, mux_state, USB_PD_MUX_HPD_LVL);
	SET_MUX_STATE_IF(status.dp_irq, mux_state, USB_PD_MUX_HPD_IRQ);
	SET_MUX_STATE_IF(status.tbt, mux_state, USB_PD_MUX_TBT_COMPAT_ENABLED);
	SET_MUX_STATE_IF(status.usb4, mux_state, USB_PD_MUX_USB4_ENABLED);
	return 0;
}

static int command_typec(const struct shell *sh, int argc, const char **argv)
{
	char *e;
	int port;
	mux_state_t mux_state = 0;

	if (argc < 2) {
		shell_error(sh, "usage: typec <port>");
		return EC_ERROR_PARAM_COUNT;
	}

	port = strtoi(argv[1], &e, 10);
	if (*e || port >= board_get_usb_pd_port_count())
		return EC_ERROR_PARAM1;

	pdc_pmc_alt_mode_get_mux(port, &mux_state);
	shell_fprintf(sh, SHELL_INFO,
		      "Port %d: USB=%d DP=%d POLARITY=%s HPD_IRQ=%d "
		      "HPD_LVL=%d SAFE=%d TBT=%d USB4=%d\n",
		      port, !!(mux_state & USB_PD_MUX_USB_ENABLED),
		      !!(mux_state & USB_PD_MUX_DP_ENABLED),
		      mux_state & USB_PD_MUX_POLARITY_INVERTED ? "INVERTED" :
								 "NORMAL",
		      !!(mux_state & USB_PD_MUX_HPD_IRQ),
		      !!(mux_state & USB_PD_MUX_HPD_LVL),
		      !!(mux_state & USB_PD_MUX_SAFE_MODE),
		      !!(mux_state & USB_PD_MUX_TBT_COMPAT_ENABLED),
		      !!(mux_state & USB_PD_MUX_USB4_ENABLED));

	return EC_SUCCESS;
}
SHELL_CMD_REGISTER(typec, NULL, "Gets typec port status.", command_typec);
#endif
