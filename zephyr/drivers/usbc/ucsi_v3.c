/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>

#include <drivers/ucsi_v3.h>

const char *const ucsi_invalid_name = "OUTSIDE_VALID_RANGE";
const char *const ucsi_deprecated_name = "DEPRECATED";

static const char *const ucsi_command_names[] = {
	[UCSI_PPM_RESET] = "PPM_RESET",
	[UCSI_CANCEL] = "CANCEL",
	[UCSI_CONNECTOR_RESET] = "CONNECTOR_RESET",
	[UCSI_ACK_CC_CI] = "ACK_CC_CI",
	[UCSI_SET_NOTIFICATION_ENABLE] = "SET_NOTIFICATION_ENABLE",
	[UCSI_GET_CAPABILITY] = "GET_CAPABILITY",
	[UCSI_GET_CONNECTOR_CAPABILITY] = "GET_CONNECTOR_CAPABILITY",
	[UCSI_SET_CCOM] = "SET_CCOM",
	[UCSI_SET_UOR] = "SET_UOR",
	[UCSI_SET_PDR] = "SET_PDR",
	[UCSI_GET_ALTERNATE_MODES] = "GET_ALTERNATE_MODES",
	[UCSI_GET_CAM_SUPPORTED] = "GET_CAM_SUPPORTED",
	[UCSI_GET_CURRENT_CAM] = "GET_CURRENT_CAM",
	[UCSI_SET_NEW_CAM] = "SET_NEW_CAM",
	[UCSI_GET_PDOS] = "GET_PDOS",
	[UCSI_GET_CABLE_PROPERTY] = "GET_CABLE_PROPERTY",
	[UCSI_GET_CONNECTOR_STATUS] = "GET_CONNECTOR_STATUS",
	[UCSI_GET_ERROR_STATUS] = "GET_ERROR_STATUS",
	[UCSI_SET_POWER_LEVEL] = "SET_POWER_LEVEL",
	[UCSI_GET_PD_MESSAGE] = "GET_PD_MESSAGE",
	[UCSI_GET_ATTENTION_VDO] = "GET_ATTENTION_VDO",
	[UCSI_GET_CAM_CS] = "GET_CAM_CS",
	[UCSI_LPM_FW_UPDATE_REQUEST] = "LPM_FW_UPDATE_REQUEST",
	[UCSI_SECURITY_REQUEST] = "SECURITY_REQUEST",
	[UCSI_SET_RETIMER_MODE] = "SET_RETIMER_MODE",
	[UCSI_SET_SINK_PATH] = "SET_SINK_PATH",
	[UCSI_SET_PDOS] = "SET_PDOS",
	[UCSI_READ_POWER_LEVEL] = "READ_POWER_LEVEL",
	[UCSI_CHUNKING_SUPPORT] = "CHUNKING_SUPPORTED",
	[UCSI_VENDOR_DEFINED_COMMAND] = "VENDOR_DEFINED",
	[UCSI_SET_USB] = "SET_USB",
	[UCSI_GET_LPM_PPM_INFO] = "GET_LPM_PPM_INFO",
};

const char *const get_ucsi_command_name(enum ucsi_command_t cmd)
{
	if (cmd >= UCSI_CMD_MAX) {
		return ucsi_invalid_name;
	} else if (!ucsi_command_names[cmd]) {
		return ucsi_deprecated_name;
	} else {
		return ucsi_command_names[cmd];
	}
}

#define CONN_CHANGE_BUF_SIZE 120
char *get_conn_status_change_bits(uint16_t raw_conn_status_change_bits)
{
	static char change_bits_buf[CONN_CHANGE_BUF_SIZE];
	unsigned int offset;
	union conn_status_change_bits_t conn_status_change_bits;
	conn_status_change_bits.raw_value = raw_conn_status_change_bits;

	memset(change_bits_buf, 0, CONN_CHANGE_BUF_SIZE);
	offset = 0;
	offset += snprintf(&change_bits_buf[offset],
			   CONN_CHANGE_BUF_SIZE - offset,
			   "(0x%04x): ", raw_conn_status_change_bits);

	if (conn_status_change_bits.external_supply_change) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "ext_supply, ");
	}
	if (conn_status_change_bits.pwr_operation_mode) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "pwr_op_mode, ");
	}
	if (conn_status_change_bits.attention) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset, "attn, ");
	}
	if (conn_status_change_bits.supported_provider_caps) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "supp_prov_caps, ");
	}
	if (conn_status_change_bits.negotiated_power_level) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "neg_pwr_lvl, ");
	}
	if (conn_status_change_bits.supported_cam) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset, "supp_cam, ");
	}
	if (conn_status_change_bits.battery_charging_status) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "batt_charging, ");
	}
	if (conn_status_change_bits.connector_partner) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "conn_partner, ");
	}
	if (conn_status_change_bits.pwr_direction) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset, "pwr_dir, ");
	}
	if (conn_status_change_bits.sink_path_status_change) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "sink_path, ");
	}
	if (conn_status_change_bits.connect_change) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset,
				   "conn_change, ");
	}
	if (conn_status_change_bits.error) {
		offset += snprintf(&change_bits_buf[offset],
				   CONN_CHANGE_BUF_SIZE - offset, "error");
	}

	return change_bits_buf;
}

static const char *drp_mode_names[] = {
	"NORMAL",
	"TRY_SRC",
	"TRY_SNK",
};
BUILD_ASSERT(ARRAY_SIZE(drp_mode_names) == DRP_MAX_ENUM);

const char *get_drp_mode_name(enum drp_mode_t mode)
{
	if (mode < DRP_INVALID) {
		return drp_mode_names[mode];
	} else {
		return "INVALID DRP MODE";
	}
}

static const char *ccom_name[] = {
	"CCOM_RP",
	"CCOM_RD",
	"CCOM_DRP",
};

const char *get_ccom_name(enum ccom_t ccom)
{
	if (ccom <= CCOM_DRP) {
		return ccom_name[ccom];
	} else {
		return "INVALID CCOM";
	}
}
