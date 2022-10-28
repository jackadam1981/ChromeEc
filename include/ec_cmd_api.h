/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A CROS_EC_COMMAND macro must be defined before including this file
 * with the following signature:
 *
 * int CROS_EC_COMMAND(int command, int version,
 *		    const void *outdata, int outsize,
 *		    void *indata, int insize)
 */

#include "ec_cmd_api-generated.h"

#define _EC_F_PR(_cmd, _v, _fn, _in, _out)	\
static inline int ec_cmd_##_fn( \
	const struct ec_params_##_in *p, \
	struct ec_response_##_out *r) \
{ \
	return CROS_EC_COMMAND(EC_CMD_##_cmd, (_v),	\
		   p, sizeof(*p), \
		   r, sizeof(*r)); \
}

#define _EC_F_P(_cmd, _v, _fn, _in)		\
static inline int ec_cmd_##_fn( \
	const struct ec_params_##_in *p)	\
{ \
	return CROS_EC_COMMAND(EC_CMD_##_cmd, (_v),	\
		   p, sizeof(*p), \
		   NULL, 0); \
}

#define _EC_F_R(_cmd, _v, _fn, _out)		\
static inline int ec_cmd_##_fn( \
	struct ec_response_##_out *r) \
{ \
	return CROS_EC_COMMAND(EC_CMD_##_cmd, (_v),	\
		   NULL, 0, \
		   r, sizeof(*r)); \
}

/*_EC_F_PR(GET_CROSS_BOARD_INFO, get_cross_board_info, get_cbi, uint32_t)*/

/*_EC_F_R(MKBP_INFO, mkbp_info, _mkbp_info)*/

_EC_F_R(HOST_EVENT_GET_B, 0, host_event_get_b, host_event_mask)
_EC_F_P(HOST_EVENT_CLEAR_B, 0, host_event_clear_b, host_event_mask)
_EC_F_P(BATTERY_CUT_OFF, 1, battery_cut_off_v1, battery_cutoff)
_EC_F_PR(MOTION_SENSE_CMD, 2, motion_sense_cmd_v2, motion_sense, motion_sense)
_EC_F_PR(USB_PD_CONTROL, 2, usb_pd_control_v2,
	 usb_pd_control, usb_pd_control_v2)
_EC_F_PR(GET_CMD_VERSIONS, 1, get_cmd_versions_v1,
	 get_cmd_versions_v1, get_cmd_versions)
_EC_F_PR(FLASH_PROTECT, 1, flash_protect_v1,
	 flash_protect, flash_protect)
_EC_F_PR(FLASH_REGION_INFO, 1, flash_region_info_v1,
	 flash_region_info, flash_region_info)
_EC_F_R(RTC_GET_VALUE, 0, rtc_get_value, rtc)
_EC_F_R(GET_BOARD_VERSION, 0, get_board_version, board_version)
_EC_F_R(GET_UPTIME_INFO, 0, get_uptime_info, uptime_info)
_EC_F_P(OVERRIDE_DEDICATED_CHARGER_LIMIT, 0, override_dedicated_charger_limit,
	dedicated_charger_limit)
_EC_F_R(GET_KEYBD_CONFIG, 0, get_keybd_config, keybd_config)

static inline int ec_cmd_get_cros_board_info(
	const struct ec_params_get_cbi *p,
	uint32_t *r)
{
	return CROS_EC_COMMAND(EC_CMD_GET_CROS_BOARD_INFO, 0,
		   p, sizeof(*p),
		   r, sizeof(*r));
}

static inline int ec_cmd_usb_pd_get_amode(
	const struct ec_params_usb_pd_get_mode_request *p,
	struct ec_params_usb_pd_get_mode_response *r)
{
	return CROS_EC_COMMAND(EC_CMD_USB_PD_GET_AMODE, 0,
		   p, sizeof(*p),
		   r, sizeof(*r));
}

static inline int ec_cmd_get_sku_id(
	struct ec_sku_id_info *r)
{
	return CROS_EC_COMMAND(EC_CMD_GET_SKU_ID, 0,
			       NULL, 0,
			       r, sizeof(*r));
}

static inline int ec_cmd_set_sku_id(
	const struct ec_sku_id_info *p)
{
	return CROS_EC_COMMAND(EC_CMD_SET_SKU_ID, 0,
			       p, sizeof(*p),
			       NULL, 0);
}
