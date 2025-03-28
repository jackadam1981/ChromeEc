/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppm_common.h"
#include "ppm_utils.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include <drivers/ucsi_v3.h>

/**
 * @brief Helper function for getting a device pointer to the PPM device
 *
 * @return NULL if device is not ready
 * @return device pointer on success
 */
static const struct device *get_ppm_dev(void)
{
	const struct device *ppm_dev = DEVICE_DT_GET(DT_INST(0, ucsi_ppm));

	if (!device_is_ready(ppm_dev)) {
		return NULL;
	}

	return ppm_dev;
}

/**
 * @brief Helper function to parse and validate a type-C port number
 *
 * @param sh Pointer to Zephyr shell object
 * @param ppm_dev Pointer to a PPM device ("usci-ppm" compat string). Used to
 *        get the number of active ports
 * @param arg_val Pointer to string to parse
 * @param port[out] Output pointer to write port number to, if successful
 *
 * @return 0 on success
 * @return -EINVAL if port number string cannot be parsed or is out of range
 */
static int cmd_get_pd_port(const struct shell *sh, const struct device *ppm_dev,
			   char *arg_val, uint8_t *port)
{
	const struct ucsi_pd_driver *ppm_api = ppm_dev->api;
	char *e;

	*port = strtoul(arg_val, &e, 0);
	if (*e || *port >= ppm_api->get_active_port_count(ppm_dev)) {
		shell_error(sh, "Invalid port");
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Run the GET_ALTERNATE_MODES UCSI command
 */
static int cmd_get_alt_modes(const struct shell *sh, int argc, char **argv)
{
	const struct device *dev = get_ppm_dev();
	uint8_t port;
	uint8_t recipient = 1; /* Default to SOP as recipient */
	struct ucsi_altmode_field altmodes[16] = { 0 };
	int num_alt_modes = 0;
	int rv;

	__ASSERT(dev, "PPM device is not ready");

	if (cmd_get_pd_port(sh, dev, argv[1], &port)) {
		shell_error(sh, "Invalid port");
		return -ERANGE;
	}

	if (argc > 2) {
		if (!strncmp(argv[2], "conn", strlen("conn"))) {
			recipient = 0;
		} else if (!strncmp(argv[2], "sopprimeprime",
				    strlen("sopprimeprime"))) {
			recipient = 3;
		} else if (!strncmp(argv[2], "sopprime", strlen("sopprime"))) {
			recipient = 2;
		} else if (!strncmp(argv[2], "sop", strlen("sop"))) {
			recipient = 1;
		} else {
			shell_error(sh, "Invalid recipient");
			return 1;
		}
	}

	rv = ppm_get_alternate_modes(dev, port + 1, recipient,
				     ARRAY_SIZE(altmodes), altmodes,
				     &num_alt_modes);
	if (rv != 0) {
		return rv;
	}

	/* Print alternate mode info in a table */

	shell_info(sh, "Port: C%u (UCSI port %u), Recipient: %u\n", port,
		   port + 1, recipient);

	if (num_alt_modes == 0) {
		shell_info(sh, "No alternate modes reported");
		return 0;
	}

	shell_info(sh, "Offset | Alternate mode");
	shell_info(sh, "-------+--------------------------------------");

	for (int i = 0; i < num_alt_modes; i++) {
		shell_info(sh, "%03d    | SVID=0x%04x MID=0x%08x", i,
			   altmodes[i].svid, altmodes[i].mid);
	}

	return 0;
}

/**
 * @brief Run the GET_CAM_SUPPORTED UCSI command
 */
static int cmd_get_cam_supported(const struct shell *sh, int argc, char **argv)
{
	const struct device *dev = get_ppm_dev();
	const struct ucsi_pd_driver *ppm_api;
	uint8_t port;
	uint8_t resp[8] = { 0 };
	size_t resp_size;
	int rv;

	__ASSERT(dev, "PPM device is not ready");

	ppm_api = dev->api;

	if (cmd_get_pd_port(sh, dev, argv[1], &port)) {
		shell_error(sh, "Invalid port");
		return -ERANGE;
	}

	rv = ppm_get_cam_supported(dev, port + 1, ARRAY_SIZE(resp), resp,
				   &resp_size);

	if (rv != 0) {
		return rv;
	}

	shell_info(sh, "Port: C%u (UCSI port %u), Supported:", port, port + 1);

	if (resp_size == 0) {
		shell_fprintf(sh, SHELL_INFO, " none");
		return 0;
	}

	shell_hexdump(sh, resp, resp_size);
	shell_fprintf(sh, SHELL_INFO, "\nSupported indexes: ");
	for (int i = 0; i < resp_size; i++) {
		for (int j = 0; j < 8; j++) {
			if (resp[i] & BIT(j)) {
				shell_fprintf(sh, SHELL_INFO, "%02d ",
					      8 * i + j);
			}
		}
	}
	shell_fprintf(sh, SHELL_INFO, "\n");

	return 0;
}

/**
 * @brief Run the GET_CURRENT_CAM UCSI command
 */
static int cmd_get_current_cam(const struct shell *sh, int argc, char **argv)
{
	const struct device *dev = get_ppm_dev();
	uint8_t port;
	uint8_t resp[8] = { 0 };
	size_t num_altmodes;
	int rv;

	__ASSERT(dev, "PPM device is not ready");

	if (cmd_get_pd_port(sh, dev, argv[1], &port)) {
		shell_error(sh, "Invalid port");
		return -ERANGE;
	}

	rv = ppm_get_current_cam(dev, port + 1, sizeof(resp), resp,
				 &num_altmodes);

	if (rv != 0) {
		return rv;
	}

	shell_info(sh, "Port: C%u (UCSI port %u), CAM:", port, port + 1);

	/* UCSI is ambiguous as to whether data is returned when there
	 * are no active alternate modes. Check for no data returned.
	 */
	if (num_altmodes == 0) {
		shell_info(sh, "No active alternate modes");
		return 0;
	}

	shell_hexdump(sh, resp, num_altmodes);

	/* UCSI returned one mode, but the encoding means that no modes
	 * are active.
	 */
	if (resp[0] == 0xFF) {
		shell_info(sh, "No active alternate modes");
		return 0;
	}

	return 0;
}

/**
 * @brief Run the SET_NEW_CAM UCSI command
 */
static int cmd_set_new_cam(const struct shell *sh, int argc, char **argv)
{
	const struct device *dev = get_ppm_dev();
	const struct ucsi_pd_driver *ppm_api;
	uint8_t port;
	int rv;
	char *e;

	__ASSERT(dev, "PPM device is not ready");

	ppm_api = dev->api;

	if (cmd_get_pd_port(sh, dev, argv[1], &port)) {
		shell_error(sh, "Invalid port");
		return -ERANGE;
	}

	/* Parse CAM fields */

	uint8_t new_cam = strtoul(argv[2], &e, 0);
	if (*e) {
		shell_error(sh, "Invalid new CAM param");
		return -EINVAL;
	}

	uint32_t am_specific = strtoul(argv[3], &e, 0);
	if (*e) {
		shell_error(sh, "Invalid AM-specific param");
		return -EINVAL;
	}

	bool enter;
	if (!strncmp(argv[4], "enter", strlen("enter"))) {
		enter = true;
	} else if (!strncmp(argv[4], "exit", strlen("exit"))) {
		enter = false;
	} else {
		shell_error(sh, "Invalid enter/exit action");
		return -EINVAL;
	}

	rv = ppm_set_new_cam(dev, port + 1, enter, new_cam, am_specific);

	if (rv == 0) {
		shell_info(sh, "Port C%d: SET_NEW_CAM %d %d 0x%08x successful",
			   port, new_cam, enter, am_specific);
	}

	return rv;
}

/* LCOV_EXCL_START */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ppm_cmds,
	SHELL_CMD_ARG(
		get_alt_modes, NULL,
		"Run GET_ALTERNATE_MODES UCSI command. Gets up to 8 responses. "
		"Default recipient is SOP.\n"
		"Usage: ppm get_alt_modes <port> "
		"[conn|sop|sopprime|sopprimeprime]",
		cmd_get_alt_modes, 2, 1),
	SHELL_CMD_ARG(get_cam_supported, NULL,
		      "Run GET_CAM_SUPPORTED UCSI command.\n"
		      "Usage: ppm get_cam_supported <port>",
		      cmd_get_cam_supported, 2, 0),
	SHELL_CMD_ARG(get_current_cam, NULL,
		      "Run GET_CURRENT_CAM UCSI command.\n"
		      "Usage: ppm get_current_cam <port>",
		      cmd_get_current_cam, 2, 0),
	SHELL_CMD_ARG(set_new_cam, NULL,
		      "Run SET_NEW_CAM UCSI command. Altmodes are described as "
		      "indexes into the GET_ALTERNATE_MODES response.\n"
		      "Usage: ppm set_new_cam <port> <new_cam> <am_specific> "
		      "<enter|exit>",
		      cmd_set_new_cam, 5, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ppm, &sub_ppm_cmds, "PPM console commands", NULL);

/* LCOV_EXCL_STOP */
