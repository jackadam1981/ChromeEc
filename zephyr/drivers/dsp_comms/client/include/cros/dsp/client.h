/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_CLIENT_H_
#define EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_CLIENT_H_

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include "ec_commands.h"
#include "proto/ec_dsp.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dsp_client_api {
  int (*get_cbi_flags)(const struct device* dev,
                       cros_dsp_comms_CbiFlag flag,
                       cros_dsp_comms_GetCbiFlagsResponse* mem);
};

int dsp_client_get_cbi_flags(const struct device* dev,
                             cros_dsp_comms_CbiFlag flag,
                             cros_dsp_comms_GetCbiFlagsResponse* mem);

int cbi_remote_get_board_info(enum cbi_data_tag tag,
                              uint8_t* buffer,
                              uint8_t* buffer_size);

void remote_lid_switch_set(bool is_open);

void remote_tablet_switch_set(bool is_360);

extern const struct device* default_client_device;

struct dsp_client_config {
  struct i2c_dt_spec i2c;
  struct gpio_dt_spec interrupt;
};

struct dsp_client_data {
  struct k_mutex mutex;
  struct k_event response_ready_event;
  struct k_work read_status_work;
  const struct dsp_client_config* config;
  struct gpio_callback gpio_cb;
  int interrupt_config;
  uint32_t pending_response_length;
  cros_dsp_comms_EcService service;
  uint8_t request_buffer[cros_dsp_comms_EcService_size];
  uint8_t response_buffer[CONFIG_PLATFORM_EC_DSP_RESPONSE_BUFFER_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif /* EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_CLIENT_H_ */
