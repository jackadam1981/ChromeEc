/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/ucsi_v3.h"
#include "zephyr/sys/util.h"
#include "zephyr/sys/util_macro.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(ucsi, NULL, NULL, NULL, NULL, NULL);

ZTEST_USER(ucsi, test_ucsi_command_names)
{
	enum ucsi_command_t cmd;
	const char *name;

	for (cmd = UCSI_PPM_RESET; cmd <= UCSI_GET_LPM_PPM_INFO; cmd++) {
		if (cmd == 0x0a || cmd == 0x17) {
			/* Skip obsolete and reserved cmds */
			continue;
		}

		name = get_ucsi_command_name(cmd);

		switch (cmd) {
		case UCSI_PPM_RESET:
			zassert_equal(strcmp(name, "PPM_RESET"), 0);
			break;
		case UCSI_CANCEL:
			zassert_equal(strcmp(name, "CANCEL"), 0);
			break;
		case UCSI_CONNECTOR_RESET:
			zassert_equal(strcmp(name, "CONNECTOR_RESET"), 0);
			break;
		case UCSI_ACK_CC_CI:
			zassert_equal(strcmp(name, "ACK_CC_CI"), 0);
			break;
		case UCSI_SET_NOTIFICATION_ENABLE:
			zassert_equal(strcmp(name, "SET_NOTIFICATION_ENABLE"),
				      0);
			break;
		case UCSI_GET_CAPABILITY:
			zassert_equal(strcmp(name, "GET_CAPABILITY"), 0);
			break;
		case UCSI_GET_CONNECTOR_CAPABILITY:
			zassert_equal(strcmp(name, "GET_CONNECTOR_CAPABILITY"),
				      0);
			break;
		case UCSI_SET_CCOM:
			zassert_equal(strcmp(name, "SET_CCOM"), 0);
			break;
		case UCSI_SET_UOR:
			zassert_equal(strcmp(name, "SET_UOR"), 0);
			break;
		case UCSI_SET_PDR:
			zassert_equal(strcmp(name, "SET_PDR"), 0);
			break;
		case UCSI_GET_ALTERNATE_MODES:
			zassert_equal(strcmp(name, "GET_ALTERNATE_MODES"), 0);
			break;
		case UCSI_GET_CAM_SUPPORTED:
			zassert_equal(strcmp(name, "GET_CAM_SUPPORTED"), 0);
			break;
		case UCSI_GET_CURRENT_CAM:
			zassert_equal(strcmp(name, "GET_CURRENT_CAM"), 0);
			break;
		case UCSI_SET_NEW_CAM:
			zassert_equal(strcmp(name, "SET_NEW_CAM"), 0);
			break;
		case UCSI_GET_PDOS:
			zassert_equal(strcmp(name, "GET_PDOS"), 0);
			break;
		case UCSI_GET_CABLE_PROPERTY:
			zassert_equal(strcmp(name, "GET_CABLE_PROPERTY"), 0);
			break;
		case UCSI_GET_CONNECTOR_STATUS:
			zassert_equal(strcmp(name, "GET_CONNECTOR_STATUS"), 0);
			break;
		case UCSI_GET_ERROR_STATUS:
			zassert_equal(strcmp(name, "GET_ERROR_STATUS"), 0);
			break;
		case UCSI_SET_POWER_LEVEL:
			zassert_equal(strcmp(name, "SET_POWER_LEVEL"), 0);
			break;
		case UCSI_GET_PD_MESSAGE:
			zassert_equal(strcmp(name, "GET_PD_MESSAGE"), 0);
			break;
		case UCSI_GET_ATTENTION_VDO:
			zassert_equal(strcmp(name, "GET_ATTENTION_VDO"), 0);
			break;
		case UCSI_GET_CAM_CS:
			zassert_equal(strcmp(name, "GET_CAM_CS"), 0);
			break;
		case UCSI_LPM_FW_UPDATE_REQUEST:
			zassert_equal(strcmp(name, "LPM_FW_UPDATE_REQUEST"), 0);
			break;
		case UCSI_SECURITY_REQUEST:
			zassert_equal(strcmp(name, "SECURITY_REQUEST"), 0);
			break;
		case UCSI_SET_RETIMER_MODE:
			zassert_equal(strcmp(name, "SET_RETIMER_MODE"), 0);
			break;
		case UCSI_SET_SINK_PATH:
			zassert_equal(strcmp(name, "SET_SINK_PATH"), 0);
			break;
		case UCSI_SET_PDOS:
			zassert_equal(strcmp(name, "SET_PDOS"), 0);
			break;
		case UCSI_READ_POWER_LEVEL:
			zassert_equal(strcmp(name, "READ_POWER_LEVEL"), 0);
			break;
		case UCSI_CHUNKING_SUPPORT:
			zassert_equal(strcmp(name, "CHUNKING_SUPPORTED"), 0);
			break;
		case UCSI_VENDOR_DEFINED_COMMAND:
			zassert_equal(strcmp(name, "VENDOR_DEFINED"), 0);
			break;
		case UCSI_SET_USB:
			zassert_equal(strcmp(name, "SET_USB"), 0);
			break;
		case UCSI_GET_LPM_PPM_INFO:
			zassert_equal(strcmp(name, "GET_LPM_PPM_INFO"), 0);
			break;
		default:
			/* Unhandled UCSI command */
			zassert_true(false);
			break;
		}
	}
}
