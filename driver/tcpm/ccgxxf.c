/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB Power delivery port management For Cypress EZ-PD CCG6DF, CCG6SF
 * CCGXXF FW is designed to adapt standard TCPM driver procedures.
 */
#include "ccgxxf.h"
#include "console.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/*
 * TODO: F/W upgrade module. move it to depthcharge.
 * Caution: Do not upload binary if it's propritery
 */
extern const uint8_t fw_bin1[];
extern const uint8_t fw_bin1_meta[];

extern const uint8_t fw_bin2[];
extern const uint8_t fw_bin2_meta[];

static int ccgxxf_get_fwu_status_reg(int port, int delay_ms, uint16_t *status)
{
	int rv, i;

	for (i = 0; i < CCGXXF_FWU_RES_RETRY; i++) {
		msleep(delay_ms);
		rv = tcpc_read16(port, CCGXXF_REG_FWU_RESPONSE, (int *) status);
		if (rv)
			return rv;

		if (*status == CCGXXF_FWU_RES_CMD_FAILED)
			return EC_ERROR_UNKNOWN;

		if (*status == CCGXXF_FWU_RES_CMD_IN_PROGRESS)
			continue;

		/* Got the status */
		return EC_SUCCESS;
	}

	return EC_ERROR_UNKNOWN;
}

static int ccgxxf_update_fw_region(int port, int fw_rows, const uint8_t *fw_bin)
{
	int rv, i;
	uint16_t status;

	for (i = 0; i < fw_rows; i++, fw_bin += CCGXXF_FWU_BUFFER_ROW_SIZE) {
		rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
				CCGXXF_FWU_CMD_WRITE_FLASH_ROW);
		if (rv)
			return rv;

		rv = ccgxxf_get_fwu_status_reg(port, 5, &status);
		if (rv)
			return rv;

		rv = tcpc_write_block(port, CCGXXF_REG_FWU_BUFFER, fw_bin,
				CCGXXF_FWU_BUFFER_ROW_SIZE);
		if (rv)
			return rv;
		msleep(20);
	}

	return EC_SUCCESS;
}

static int ccgxxf_fw_update(int port)
{
	int rv, read_val;
	const uint8_t *fw_bin = fw_bin1;
	uint8_t fw_ver_major, fw_ver_minor;
	uint16_t fw_build, fwu_status;

	/* Make sure its correct vendor ID */
	rv = tcpc_read16(port, TCPC_REG_VENDOR_ID, &read_val);
	if (rv)
		return rv;
	if (read_val != CCGXXF_VENDOR_ID) {
		CPRINTS("Invalid vendor ID");
		return EC_ERROR_UNKNOWN;
	}

	/* Make sure it has correct product ID */
	rv = tcpc_read16(port, TCPC_REG_PRODUCT_ID, &read_val);
	if (rv)
		return rv;
	if (read_val != CCGXXF_PRODUCT_ID_CCG6DF &&
		read_val != CCGXXF_PRODUCT_ID_CCG6SF) {
		CPRINTS("Invalid product ID");
		return EC_ERROR_UNKNOWN;
	}

	/* Get the F/W version info from the device */
	rv = tcpc_read16(port, CCGXXF_REG_FW_VERSION, &read_val);
	if (rv)
		return rv;
	fw_ver_major = (read_val >> 8) & 0x00FF;
	fw_ver_minor = read_val & 0x00FF;

	rv = tcpc_read16(port, CCGXXF_REG_FW_VERSION_BUILD, (int *) &fw_build);
	if (rv)
		return rv;

	CPRINTS("FW Version from the device: %d.%d.%d",
			fw_ver_major, fw_ver_minor, fw_build);

	CPRINTS("FW Version from the binary: %d.%d.%d",
			fw_bin[CCGXXF_BUILD_VER_MAJOR_OFFSET],
			fw_bin[CCGXXF_BUILD_VER_MINOR_OFFSET],
			fw_bin[CCGXXF_BUILD_NUM_OFFSET]);

	/* Nothing to do if the FW version of the binary is same as in device */
	if (fw_ver_major == fw_bin[CCGXXF_BUILD_VER_MAJOR_OFFSET] &&
		fw_ver_minor == fw_bin[CCGXXF_BUILD_VER_MINOR_OFFSET] &&
		fw_build == fw_bin[CCGXXF_BUILD_NUM_OFFSET])
		return EC_SUCCESS;

	/* Enable Firmware Upgrade Mode */
	rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND, CCGXXF_FWU_CMD_ENABLE);
	if (rv)
		return rv;

	rv = ccgxxf_get_fwu_status_reg(port, 5, &fwu_status);
	if (rv)
		return rv;
	if (fwu_status != CCGXXF_FWU_RES_SUCCESS)
		return EC_ERROR_UNKNOWN;

	/* Get the F/W region to be updated */
	rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_GET_FW_MODE);
	if (rv)
		return rv;

	rv = ccgxxf_get_fwu_status_reg(port, 5, &fwu_status);
	if (rv)
		return rv;

	/* Update the F/W region & metadata */
	if (fwu_status == CCGXXF_FWU_RES_FW_REGION_1) {
		rv = ccgxxf_update_fw_region(port, CCGXXF_FWU_BUFFER_ROWS,
						fw_bin1);
		if (rv)
			return rv;
		rv = ccgxxf_update_fw_region(port, 1, fw_bin1_meta);
	} else if (fwu_status == CCGXXF_FWU_RES_FW_REGION_2) {
		rv = ccgxxf_update_fw_region(port, CCGXXF_FWU_BUFFER_ROWS,
						fw_bin2);
		if (rv)
			return rv;
		rv = ccgxxf_update_fw_region(port, 1, fw_bin2_meta);
	} else {
		return EC_ERROR_UNKNOWN;
	}

	if (rv)
		return rv;

	/* Validate f/w image */
	rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_VALIDATE_FW_IMAGE);
	if (rv)
		return rv;
	rv = ccgxxf_get_fwu_status_reg(port, 50, &fwu_status);
	if (rv)
		return rv;
	if (fwu_status != CCGXXF_FWU_RES_SUCCESS)
		return EC_ERROR_UNKNOWN;

	rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_DISABLE);
	if (rv)
		return rv;
	msleep(20);

	rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
			CCGXXF_FWU_CMD_RESET);
	if (rv)
		return rv;
	msleep(20);

	return EC_SUCCESS;
}

static int console_command_ccgxxf_fw_update(int argc, char **argv)
{
	char *e;
	int port;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	/* get & vaidate port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || !board_is_usb_pd_port_present(port))
		return EC_ERROR_PARAM1;

	return ccgxxf_fw_update(port);
}
DECLARE_CONSOLE_COMMAND(ccg_fw, console_command_ccgxxf_fw_update,
			"port", "Update ccgxxf firmware");
