/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "uart.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <drivers/pdc.h>
#include <usbc/pdc_power_mgmt.h>

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

const uint32_t pdc_src_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};

const uint32_t pdc_src_pdo_max[] = {
	PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
};



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

static int cmd_pdc_get_status(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	enum pd_power_role pr;
	enum pd_data_role dr;
	enum tcpc_cc_polarity polarity;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	/* Get PDC Status */
	pr = pdc_power_mgmt_get_power_role(port);
	dr = pdc_power_mgmt_pd_get_data_role(port);
	polarity = pdc_power_mgmt_pd_get_polarity(port);
	shell_fprintf(sh, SHELL_INFO,
		      "Port C%d CC%d, Role: %s-%s PDC State: %s"
		      "\n",
		      port, polarity, pr == PD_ROLE_SINK ? "SNK" : "SRC",
		      dr == PD_ROLE_DFP ? "DFP" : "UFP",
		      pdc_power_mgmt_get_task_state_name(port));

	return EC_SUCCESS;
}

static int cmd_pdc_get_info(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	struct pdc_info_t pdc_info = { 0 };

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	/* Get PDC Status */
	rv = pdc_power_mgmt_get_info(port, &pdc_info);
	if (rv) {
		shell_error(sh, "Could not get port %u info (%d)", port, rv);
		return rv;
	}

	shell_fprintf(sh, SHELL_INFO,
		      "FW Ver: %u.%u.%u\n"
		      "PD Rev: %u\n"
		      "PD Ver: %u\n"
		      "VID/PID: %04x:%04x\n"
		      "Running Flash Code: %c\n"
		      "Flash Bank: %u\n",
		      PDC_FWVER_GET_MAJOR(pdc_info.fw_version),
		      PDC_FWVER_GET_MINOR(pdc_info.fw_version),
		      PDC_FWVER_GET_PATCH(pdc_info.fw_version),
		      pdc_info.pd_revision, pdc_info.pd_version,
		      PDC_VIDPID_GET_VID(pdc_info.vid_pid),
		      PDC_VIDPID_GET_PID(pdc_info.vid_pid),
		      pdc_info.is_running_flash_code ? 'Y' : 'N',
		      pdc_info.running_in_flash_bank);

	return EC_SUCCESS;
}

static int cmd_pdc_prs(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	/* Trigger power role swap request */
	pdc_power_mgmt_request_power_swap(port);

	return EC_SUCCESS;
}

static int cmd_pdc_drs(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	/* Verify port partner supports data role swaps */
	if (!pdc_power_mgmt_get_partner_data_swap_capable(port)) {
		shell_error(sh, "Port partner doesn't support drs");
		return -EIO;
	}

	/* Trigger data role swap request */
	pdc_power_mgmt_request_data_swap(port);

	return EC_SUCCESS;
}

static int cmd_pdc_dualrole(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	enum pd_dual_role_states state;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	if (!strcmp(argv[2], "on")) {
		state = PD_DRP_TOGGLE_ON;
	} else if (!strcmp(argv[2], "off")) {
		state = PD_DRP_TOGGLE_OFF;
	} else if (!strcmp(argv[2], "freeze")) {
		state = PD_DRP_FREEZE;
	} else if (!strcmp(argv[2], "sink")) {
		state = PD_DRP_FORCE_SINK;
	} else if (!strcmp(argv[2], "source")) {
		state = PD_DRP_FORCE_SOURCE;
	} else {
		shell_error(sh, "Invalid dualrole mode");
		return -EINVAL;
	}

	pdc_power_mgmt_set_dual_role(port, state);

	return EC_SUCCESS;
}

static int cmd_pdc_reset(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t port;
	int rv;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	/* Trigger a PDC reset for this port. */
	rv = pdc_power_mgmt_reset(port);
	if (rv) {
		shell_error(sh, "Could not reset port %u (%d)", port, rv);
		return rv;
	}

	return EC_SUCCESS;
}

static int cmd_pdc_connector_reset(const struct shell *sh, size_t argc,
				   char **argv)
{
	int rv;
	uint8_t port;
	enum connector_reset reset_type;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	if (!strcmp(argv[2], "hard")) {
		reset_type = PD_HARD_RESET;
	} else if (!strcmp(argv[2], "data")) {
		reset_type = PD_DATA_RESET;
	} else {
		shell_error(sh, "Invalid connector reset type");
		return -EINVAL;
	}

	/* Trigger a PDC connector reset */
	rv = pdc_power_mgmt_connector_reset(port, reset_type);
	if (rv) {
		shell_error(sh, "CONNECTOR_RESET not sent to port %u (%d)",
			    port, rv);
	}

	return rv;
}

static int cmd_pdc_get_snk_caps(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	uint32_t pdos[7];
	uint8_t cnt;
	int i;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	cnt = pdc_power_mgmt_get_snk_cap_cnt(port);
	shell_fprintf(sh, SHELL_INFO,"SNK PDO cnt = %d\n", cnt);
	if (cnt) {
		memcpy(pdos, pdc_power_mgmt_get_snk_caps(port),
		       sizeof(uint32_t) * cnt);
		for (i = 0; i < cnt; i++) {
			shell_fprintf(sh, SHELL_INFO,"[SNK PDO %d]: 0x%08x\n", i,
				      pdos[i]);
		}
	}

	cnt = pdc_power_mgmt_get_snk_cap_cnt(port);
	memcpy(pdos, pdc_power_mgmt_get_lpm_src_caps(port),
		       sizeof(uint32_t) * cnt);
	if (cnt) {
		memcpy(pdos, pdc_power_mgmt_get_snk_caps(port),
		       sizeof(uint32_t) * cnt);
		for (i = 0; i < cnt; i++) {
			shell_fprintf(sh, SHELL_INFO,"[SRC PDO %d]: 0x%08x\n", i,
				      pdos[i]);
		}
	}

	return EC_SUCCESS;
}

static int cmd_pdc_get_src_caps(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	uint32_t pdos[7];
	uint8_t cnt;
	int i;
	const uint32_t *lpm_pdo;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	cnt = pdc_power_mgmt_get_src_cap_cnt(port);
	shell_fprintf(sh, SHELL_INFO,"PARTNER SRC PDO cnt = %d\n", cnt);
	if (cnt) {
		memcpy(pdos, pdc_power_mgmt_get_src_caps(port),
		       sizeof(uint32_t) * cnt);
		for (i = 0; i < cnt; i++) {
			shell_fprintf(sh, SHELL_INFO,"[SRC PDO %d]: 0x%08x\n", i,
				      pdos[i]);
		}
	}

	lpm_pdo = pdc_power_mgmt_get_lpm_src_caps(port);
	cnt = pdc_power_mgmt_get_snk_cap_cnt(port);
	memcpy(pdos, lpm_pdo,
		       sizeof(uint32_t) * cnt);

	for (i = 0; i < cnt; i++) {
		shell_fprintf(sh, SHELL_INFO,"[SRC PDO %d]: 0x%08x\n", i,
			      pdos[i]);
	}


	return EC_SUCCESS;
}

static int cmd_pdc_set_src_caps(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	const uint32_t *pdo;
	enum usb_typec_current_t tcc;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	if (!strcmp(argv[2], "hi")) {
		pdo = pdc_src_pdo_max;
		tcc = TC_CURRENT_1_5A;
	} else if (!strcmp(argv[2], "lo")) {
		pdo = pdc_src_pdo;
		tcc = TC_CURRENT_USB_DEFAULT;
	} else {
		shell_error(sh, "Invalid selection");
		return -EINVAL;
	}

	rv = pdc_power_mgmt_set_typec_curr_limit(port, tcc);
	rv = pdc_power_mgmt_set_src_pdo(port, pdo, 1);

	if (rv) {
		shell_error(sh, "SET_PDOS not sent to port %u (%d)",
			    port, rv);
	}


	return EC_SUCCESS;
}

static int cmd_pdc_set_rp(const struct shell *sh, size_t argc, char **argv)
{
	int rv;
	uint8_t port;
	const uint32_t *pdo;
	enum usb_typec_current_t tcc;

	/* Get PD port number */
	rv = cmd_get_pd_port(sh, argv[1], &port);
	if (rv)
		return rv;

	if (!strcmp(argv[2], "hi")) {
		pdo = pdc_src_pdo_max;
		tcc = TC_CURRENT_3_0A;
	} else if (!strcmp(argv[2], "med")) {
		pdo = pdc_src_pdo;
		tcc = TC_CURRENT_1_5A;
	} else if (!strcmp(argv[2], "lo")) {
		pdo = pdc_src_pdo;
		tcc = TC_CURRENT_USB_DEFAULT;
	} else {
		shell_error(sh, "Invalid selection");
		return -EINVAL;
	}

	rv = pdc_power_mgmt_set_typec_curr_limit(port, tcc);

	if (rv) {
		shell_error(sh, "SET_PDOS not sent to port %u (%d)",
			    port, rv);
	}

	return EC_SUCCESS;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_pdc_cmds,
	SHELL_CMD_ARG(status, NULL,
		      "Get PD status\n"
		      "Usage: pdc status <port>",
		      cmd_pdc_get_status, 2, 0),
	SHELL_CMD_ARG(info, NULL,
		      "Get PDC chip info\n"
		      "Usage: pdc info <port>",
		      cmd_pdc_get_info, 2, 0),
	SHELL_CMD_ARG(prs, NULL,
		      "Trigger power role swap\n"
		      "Usage: pdc prs <port>",
		      cmd_pdc_prs, 2, 0),
	SHELL_CMD_ARG(drs, NULL,
		      "Trigger data role swap\n"
		      "Usage: pdc drs <port>",
		      cmd_pdc_drs, 2, 0),
	SHELL_CMD_ARG(reset, NULL,
		      "Trigger a PDC reset\n"
		      "Usage: pdc reset <port>",
		      cmd_pdc_reset, 2, 0),
	SHELL_CMD_ARG(dualrole, NULL,
		      "Set dualrole mode\n"
		      "Usage: pdc dualrole  <port> [on|off|freeze|sink|source]",
		      cmd_pdc_dualrole, 3, 0),
	SHELL_CMD_ARG(conn_reset, NULL,
		      "Trigger hard or data reset\n"
		      "Usage: pdc conn_reset  <port> [hard|data]",
		      cmd_pdc_connector_reset, 3, 0),
	SHELL_CMD_ARG(snk_caps, NULL,
		      "Get snk caps from port partner\n"
		      "Usage: pdc snk_caps <port>",
		      cmd_pdc_get_snk_caps, 2, 0),
	SHELL_CMD_ARG(src_caps, NULL,
		      "Get src caps from port partner\n"
		      "Usage: pdc src_caps <port>",
		      cmd_pdc_get_src_caps, 2, 0),
	SHELL_CMD_ARG(src_caps_set, NULL,
		      "Set src caps from port partner\n"
		      "Usage: pdc src_caps <port>",
		      cmd_pdc_set_src_caps, 3, 0),
	SHELL_CMD_ARG(set_rp, NULL,
		      "Set Rp level\n"
		      "Usage: pdc set_rp <port> [hi|med|lo]",
		      cmd_pdc_set_rp, 3, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(pdc, &sub_pdc_cmds, "PDC console commands", NULL);

static int cmd_pd_version(const struct shell *sh, size_t argc, char **argv)
{
	shell_fprintf(sh, SHELL_INFO, "3\n");
	return EC_SUCCESS;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_pd_cmds,
			       SHELL_CMD(version, NULL,
					 "Get PD version\n"
					 "Usage: pd version",
					 cmd_pd_version),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(pd, &sub_pd_cmds, "PD commands (deprecated)", NULL);
