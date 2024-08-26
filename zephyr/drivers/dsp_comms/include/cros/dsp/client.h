/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_CLIENT_H_
#define EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_CLIENT_H_

#include "proto/ec_ish.pb.h"
#include "ec_commands.h"

#include <errno.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dsp_client_api {
	int (*get_cbi_flags)(const struct device *dev,
			     cros_dsp_comms_CbiFlag flag,
			     cros_dsp_comms_GetCbiFlagsResponse *mem);
};

int dsp_client_get_cbi_flags(const struct device *dev,
				  cros_dsp_comms_CbiFlag flag,
				  cros_dsp_comms_GetCbiFlagsResponse *mem);

int cbi_remote_get_board_info(enum cbi_data_tag tag, uint8_t *buffer, uint8_t *buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_CLIENT_H_ */
