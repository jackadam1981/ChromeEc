/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_INTERNAL_SERVICE_DRIVER_H_
#define EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_INTERNAL_SERVICE_DRIVER_H_

#include "proto/ec_ish.pb.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CROS_DSP_RESPONSE_BUFFER_SIZE 128

struct dsp_service_config {
	const struct device *bus;
	struct gpio_dt_spec interrupt;
};

struct dsp_service_data {
	struct i2c_target_config target_config;
	const struct dsp_service_config *dev_config;

	uint8_t request_buffer[cros_dsp_comms_EcService_size];
	uint32_t request_buffer_size;
	cros_dsp_comms_EcService pending_service_request;

	struct k_work get_cbi_flags_work;

	struct k_sem data_processing_semaphore;

	struct {
		uint8_t has_status_pending : 1;
		uint8_t has_response_pending : 1;
		uint8_t _reserved : 6;
	} response_state;

	uint8_t response_buffer[CROS_DSP_RESPONSE_BUFFER_SIZE];
	uint8_t response_buffer_size;
	uint8_t status_buffer[cros_dsp_comms_GetStatusResponse_size + 4];

	// pio required arguments
	uint8_t status_buffer_pos;
	uint8_t response_buffer_pos;
};

#define CROS_DSP_GPIO_ON  1
#define CROS_DSP_GPIO_OFF 0

bool dsp_service_handle_decoded_request(struct dsp_service_data *data);

bool dsp_service_attempt_to_decode(struct dsp_service_data *data);

int dsp_service_read_requested(struct i2c_target_config *cfg, uint8_t *out);
int dsp_service_read_processed(struct i2c_target_config *cfg, uint8_t *out);
int dsp_service_write_requested(struct i2c_target_config *cfg);
int dsp_service_write_received(struct i2c_target_config *cfg, uint8_t in);
int dsp_service_stop(struct i2c_target_config *cfg);
void dsp_service_buf_write_received(struct i2c_target_config *cfg, uint8_t *ptr,
				    uint32_t len);
int dsp_service_buf_read_requested(struct i2c_target_config *cfg, uint8_t **ptr,
				   uint32_t *len);

#ifdef __cplusplus
}
#endif

#endif /* EC_ZEPHYR_DRIVERS_DSP_COMMS_INCLUDE_CROS_DSP_INTERNAL_SERVICE_DRIVER_H_ */
