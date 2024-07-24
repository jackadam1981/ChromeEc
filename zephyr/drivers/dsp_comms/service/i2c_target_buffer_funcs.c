/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros/dsp/internal/service_driver.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_DECLARE(dsp_service, CONFIG_DSP_COMMS_LOG_LEVEL);

void dsp_service_buf_write_received(struct i2c_target_config *cfg, uint8_t *ptr,
                                    uint32_t len) {
  struct dsp_service_data *data =
      CONTAINER_OF(cfg, struct dsp_service_data, target_config);

  if (k_sem_take(&data->data_processing_semaphore, K_NO_WAIT) != 0) {
    LOG_ERR("Can't process a request at this time");
    return;
  }

  // Check capacity
  if (len > cros_dsp_comms_EcService_size) {
    LOG_ERR("Overflow, trying to write (%uB), capacity is %uB", len,
            cros_dsp_comms_EcService_size);
    k_sem_give(&data->data_processing_semaphore);
    return;
  }

  LOG_DBG("Writing %u bytes", len);

  // Set the size and copy the data
  data->request_buffer_size = len;
  memcpy(data->request_buffer, ptr, len);

  // Try to decode
  bool is_decoded = dsp_service_attempt_to_decode(data);

  if (!is_decoded) {
    LOG_ERR("Failed to decode message");
    k_sem_give(&data->data_processing_semaphore);
    return;
  }

  LOG_DBG("deasserting GPIO...");
  int rc = gpio_pin_set_dt(&data->dev_config->interrupt, CROS_DSP_GPIO_OFF);
  LOG_DBG("deasserting GPIO (%d)", rc);
  if (!dsp_service_handle_decoded_request(data)) {
    LOG_ERR("Failed to handle request");
    k_sem_give(&data->data_processing_semaphore);
    return;
  }
}

int dsp_service_buf_read_requested(struct i2c_target_config *cfg, uint8_t **ptr,
                                   uint32_t *len) {
  struct dsp_service_data *data =
      CONTAINER_OF(cfg, struct dsp_service_data, target_config);

  if (k_sem_take(&data->data_processing_semaphore, K_NO_WAIT) != 0) {
    return -EBUSY;
  }

  if (data->response_state.has_status_pending) {
    // Send the size of the response
    *ptr = data->status_buffer;
    *len = sizeof(((struct dsp_service_data *)0)->status_buffer);
    data->response_state.has_status_pending = 0;
    int rc = gpio_pin_set_dt(&data->dev_config->interrupt, 0);
    LOG_DBG("gpio_pin_set_dt(OFF) rc=%d", rc);
    k_sem_give(&data->data_processing_semaphore);
    return 0;
  }

  if (data->response_state.has_response_pending) {
    *ptr = data->response_buffer;
    *len = data->response_buffer_size;
    data->response_state.has_response_pending = 0;
    k_sem_give(&data->data_processing_semaphore);
    return 0;
  }

  k_sem_give(&data->data_processing_semaphore);
  return -ENODATA;
}