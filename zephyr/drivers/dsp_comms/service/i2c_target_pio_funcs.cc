/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <cerrno>

#include "cros/dsp/internal/service_driver.hh"

LOG_MODULE_DECLARE(dsp_service, CONFIG_DSP_COMMS_LOG_LEVEL);

int dsp_service_write_requested(struct i2c_target_config* cfg) {
  // We have a new request, clear the buffer.
  dsp_service_data* data = cros::dsp::util::ContainerOf(cfg, &dsp_service_data::target_config);

  LOG_DBG("Taking semaphore...");
  if (k_sem_take(&data->data_processing_semaphore, K_NO_WAIT) != 0) {
    LOG_ERR("Can't start a new write at this time");
    return -EBUSY;
  }

  data->request_buffer_size = 0;
  k_sem_give(&data->data_processing_semaphore);
  return 0;
}

int dsp_service_write_received(struct i2c_target_config* cfg, uint8_t in) {
  dsp_service_data* data = cros::dsp::util::ContainerOf(cfg, &dsp_service_data::target_config);

  LOG_DBG("Taking semaphore...");
  if (k_sem_take(&data->data_processing_semaphore, K_NO_WAIT) != 0) {
    LOG_ERR("Can't process more bytes at this time");
    return -EBUSY;
  }

  // Check that we have room
  if (data->request_buffer_size >= cros_dsp_comms_EcService_size) {
    LOG_ERR("request_buffer_size (%uB) overflow, capacity is %uB",
            data->request_buffer_size,
            cros_dsp_comms_EcService_size);
    k_sem_give(&data->data_processing_semaphore);
    return 0;
  }

  // Write the byte
  data->request_buffer[data->request_buffer_size++] = in;

  // Try to decode
  bool is_decoded = dsp_service_attempt_to_decode(data);

  if (!is_decoded) {
    // Failed to decode, wait for more bytes
    k_sem_give(&data->data_processing_semaphore);
    return 0;
  }

  LOG_DBG("deasserting GPIO...");
  int rc = gpio_pin_set_dt(&data->dev_config->interrupt, CROS_DSP_GPIO_OFF);
  LOG_DBG("deasserting GPIO (%d)", rc);
  if (!dsp_service_handle_decoded_request(data)) {
    // We did not scheduled a work item, release the semaphore
    k_sem_give(&data->data_processing_semaphore);
  }
  return 0;
}

int dsp_service_read_requested(struct i2c_target_config* cfg, uint8_t* out) {
  dsp_service_data* data = cros::dsp::util::ContainerOf(cfg, &dsp_service_data::target_config);

  if (k_sem_take(&data->data_processing_semaphore, K_NO_WAIT) != 0) {
    LOG_ERR("Can't start a read");
    return -EBUSY;
  }

  auto response = data->transport_.ReadNextMessage();
  if (!response.ok() || response->size() == 0) {
    LOG_ERR("No pending response");
    k_sem_give(&data->data_processing_semaphore);
    return -ENODATA;
  }

  data->pio_response_buffer_ = *response;
  *out = static_cast<uint8_t>(data->pio_response_buffer_[0]);
  data->pio_response_buffer_position_ = 1;

  k_sem_give(&data->data_processing_semaphore);
  return 0;
}

int dsp_service_read_processed(struct i2c_target_config* cfg, uint8_t* out) {
  dsp_service_data* data = cros::dsp::util::ContainerOf(cfg, &dsp_service_data::target_config);

  if (k_sem_take(&data->data_processing_semaphore, K_NO_WAIT) != 0) {
    LOG_ERR("Can't start a read");
    return -EBUSY;
  }

  if (data->pio_response_buffer_position_ >= data->pio_response_buffer_.size()) {
    LOG_WRN("Reading too many bytes, padding with 0");
    *out = 0;
    k_sem_give(&data->data_processing_semaphore);
    return 0;
  }

  *out = static_cast<uint8_t>(data->pio_response_buffer_[data->pio_response_buffer_position_++]);
  k_sem_give(&data->data_processing_semaphore);
  return 0;
}

int dsp_service_stop(struct i2c_target_config* cfg) {
  LOG_DBG("Done");
  return 0;
}
