/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_dsp_service

#include "cros/dsp/internal/service_driver.h"
#include "cros_board_info.h"
#include "lid_angle.h"
#include "proto/ec_ish.pb.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#include <pb_decode.h>
#include <pb_encode.h>

LOG_MODULE_REGISTER(dsp_service, CONFIG_DSP_COMMS_LOG_LEVEL);

static int dsp_service_encode_response(struct dsp_service_data *data,
                                       const void *response,
                                       const pb_msgdesc_t *fields) {
  // Attempt to encode the struct
  pb_ostream_t response_ostream = pb_ostream_from_buffer(
      data->response_buffer, CROS_DSP_RESPONSE_BUFFER_SIZE);
  bool response_encode_status = pb_encode(&response_ostream, fields, response);

  if (!response_encode_status) {
    LOG_ERR("Failed to encode GetCbiFlagsResponse");
    return -EINTR;
  }

  cros_dsp_comms_GetStatusResponse status =
      cros_dsp_comms_GetStatusResponse_init_zero;
  status.flags[0] = BIT(cros_dsp_comms_GetStatusResponse_Flag_RESPONSE_READY);
  status.response_length = response_ostream.bytes_written;

  pb_ostream_t status_ostream = pb_ostream_from_buffer(
      data->status_buffer,
      sizeof(((struct dsp_service_data *)0)->status_buffer));
  bool status_encode_success = pb_encode_delimited(
      &status_ostream, cros_dsp_comms_GetStatusResponse_fields, &status);

  if (!status_encode_success) {
    LOG_ERR("Failed to encode GetStatusResponse");
    return -EINTR;
  }
  data->response_state.has_status_pending = 1;
  data->response_state.has_response_pending = 1;
  data->response_buffer_size = response_ostream.bytes_written;
  data->response_buffer_pos = 0;
  data->status_buffer_pos = 0;

  LOG_DBG("Staged 0x%02x bytes as response", response_ostream.bytes_written);
  return 0;
}

static void dsp_service_handle_get_cbi_flags_request(struct k_work *work_item) {
  // unsigned int lock = irq_lock();
  struct dsp_service_data *data =
      CONTAINER_OF(work_item, struct dsp_service_data, get_cbi_flags_work);
  const cros_dsp_comms_GetCbiFlagsRequest *request =
      &(data->pending_service_request.request.get_cbi_flags);
  enum cbi_data_tag tag;
  int rc = 0;
  uint8_t size = CROS_DSP_RESPONSE_BUFFER_SIZE;
  cros_dsp_comms_GetCbiFlagsResponse response =
      cros_dsp_comms_GetCbiFlagsResponse_init_zero;

  LOG_DBG("GOT: GetCbiFlagsRequest");
  switch (request->which) {
  case cros_dsp_comms_CbiFlag_VERSION:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_BOARD_VERSION;
    LOG_DBG("Fetching BOARD_VERSION");
    break;
  case cros_dsp_comms_CbiFlag_OEM:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_OEM_ID;
    LOG_DBG("Fetching OEM_ID");
    break;
  case cros_dsp_comms_CbiFlag_SKU:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_SKU_ID;
    LOG_DBG("Fetching SKU_ID");
    break;
  case cros_dsp_comms_CbiFlag_MODEL:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_MODEL_ID;
    LOG_DBG("Fetching MODEL_ID");
    break;
  case cros_dsp_comms_CbiFlag_FW_CONFIG:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_FW_CONFIG;
    LOG_DBG("Fetching FW_CONFIG");
    break;
  case cros_dsp_comms_CbiFlag_PCB_SUPPLIER:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_PCB_SUPPLIER;
    LOG_DBG("Fetching PCB_SUPPLIER");
    break;
  case cros_dsp_comms_CbiFlag_SSFC:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_SSFC;
    LOG_DBG("Fetching SSFC");
    break;
  case cros_dsp_comms_CbiFlag_REWORK:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_64_tag;
    tag = CBI_TAG_REWORK_ID;
    LOG_DBG("Fetching REWORK_ID");
    break;
  case cros_dsp_comms_CbiFlag_FACTORY_CALIBRATION_DATA:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag;
    tag = CBI_TAG_FACTORY_CALIBRATION_DATA;
    LOG_DBG("Fetching FACTORY_CALIBRATION_DATA");
    break;
  case cros_dsp_comms_CbiFlag_DRAM_PART_NUM:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag;
    tag = CBI_TAG_DRAM_PART_NUM;
    LOG_DBG("Fetching DRAM_PART_NUM");
    break;
  case cros_dsp_comms_CbiFlag_OEM_NAME:
    response.which_flags = cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag;
    tag = CBI_TAG_OEM_NAME;
    LOG_DBG("Fetching OEM_NAME");
    break;
  default:
    LOG_WRN("Unsupported CBI read request");
    rc = -EINVAL;
  }

  switch (response.which_flags) {
  case cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag:
    size = 4;
    rc = cbi_get_board_info(tag, (uint8_t *)&response.flags.flags_32, &size);
    if (rc == 0 && size > 4) {
      LOG_ERR("Size is too big for buffer");
      rc = -EOVERFLOW;
    }
    LOG_DBG("32 bit fetch rc(%d), value=(0x%08x)", rc, response.flags.flags_32);
    break;
  case cros_dsp_comms_GetCbiFlagsResponse_flags_64_tag:
    size = 8;
    rc = cbi_get_board_info(tag, (uint8_t *)&response.flags.flags_64, &size);
    if (rc == 0 && size > 8) {
      LOG_ERR("Size is too big for buffer");
      rc = -EOVERFLOW;
    }
    break;
  case cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag:
    size = 80;
    rc = cbi_get_board_info(tag, (uint8_t *)response.flags.flags_string, &size);
    if (rc == 0 && size > 80) {
      LOG_ERR("Size is too big for buffer");
      rc = -EOVERFLOW;
    }
    break;
  default:
    break;
  }

  if (rc == 0) {
    // Attempt to encode
    rc = dsp_service_encode_response(data, &response,
                                     cros_dsp_comms_GetCbiFlagsResponse_fields);
  } else {
    LOG_ERR("Failed to read CBI value");
  }

  if (rc != 0) {
    cros_dsp_comms_GetStatusResponse status =
        cros_dsp_comms_GetStatusResponse_init_zero;
    status.flags[0] =
        BIT(cros_dsp_comms_GetStatusResponse_Flag_PROCESSING_ERROR);

    pb_ostream_t status_ostream = pb_ostream_from_buffer(
        data->status_buffer,
        sizeof(((struct dsp_service_data *)0)->status_buffer));
    bool status_encode_success = pb_encode_delimited(
        &status_ostream, cros_dsp_comms_GetStatusResponse_fields, &status);

    ARG_UNUSED(status_encode_success);
    __ASSERT_NO_MSG(status_encode_success);
    data->response_state.has_status_pending = 1;
    data->status_buffer_pos = 0;
  }

  k_sem_give(&data->data_processing_semaphore);
  LOG_DBG("asserting GPIO...");
  rc = gpio_pin_set_dt(&data->dev_config->interrupt, CROS_DSP_GPIO_ON);
  LOG_DBG("asserting GPIO (%d)", rc);
}

bool dsp_service_attempt_to_decode(struct dsp_service_data *data) {
  // Try to decode
  pb_istream_t istream =
      pb_istream_from_buffer(data->request_buffer, data->request_buffer_size);
  return pb_decode(&istream, cros_dsp_comms_EcService_fields,
                   &data->pending_service_request);
}

bool dsp_service_handle_decoded_request(struct dsp_service_data *data) {
  switch (data->pending_service_request.which_request) {
  case cros_dsp_comms_EcService_notify_notebook_mode_change_tag:
    LOG_DBG("GOT: NotifyNotebookModeChangeRequest");
    if (IS_ENABLED(CONFIG_PLATFORM_EC_DSP_SERVICE_REMOTE_LID_ANGLE)) {
      switch (data->pending_service_request.request.notify_notebook_mode_change
                  .new_mode) {
      case cros_dsp_comms_NotebookMode_NOTEBOOK_MODE:
        lid_angle_peripheral_enable(true);
        break;
      case cros_dsp_comms_NotebookMode_TABLET_MODE:
        lid_angle_peripheral_enable(false);
        break;
      default:
        LOG_WRN("Unsupported notebook mode");
        break;
      }
    }
    return false;
  case cros_dsp_comms_EcService_get_cbi_flags_tag:
    LOG_DBG("Scheduling get_cbi_flags_work");
    k_work_submit(&data->get_cbi_flags_work);
    return true;
  default:
    LOG_WRN("Unsupported request type");
    return false;
  }
}

static const struct i2c_target_callbacks dsp_service_callbacks = {
    .write_requested = dsp_service_write_requested,
    .read_requested = dsp_service_read_requested,
    .write_received = dsp_service_write_received,
    .read_processed = dsp_service_read_processed,
#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
    .buf_write_received = dsp_service_buf_write_received,
    .buf_read_requested = dsp_service_buf_read_requested,
#endif
    .stop = dsp_service_stop,
};

static const struct device *dsp_service_dev;

static int dsp_service_init(const struct device *dev) {
  const struct dsp_service_config *config = dev->config;
  struct dsp_service_data *data = dev->data;
  int rc = 0;
  dsp_service_dev = dev;

  k_msleep(5000);

  LOG_ERR("Initializing DSP service with callbacks @%p",
          (void *)&dsp_service_callbacks);

  k_work_init(&data->get_cbi_flags_work,
              dsp_service_handle_get_cbi_flags_request);

  k_sem_init(&data->data_processing_semaphore, 1, 1);

  rc = i2c_target_register(config->bus, &data->target_config);
  if (rc != 0) {
    LOG_ERR("Failed to configure bus as target (%d)", rc);
    return rc;
  }

  if (!gpio_is_ready_dt(&config->interrupt)) {
    LOG_ERR("GPIO is not ready");
    return -EINVAL;
  }

  rc = gpio_pin_configure_dt(&config->interrupt, GPIO_OUTPUT);
  if (rc != 0) {
    LOG_ERR("Failed to configure interrupt as output (%d)", rc);
    return rc;
  }

  // Deassert GPIO
  LOG_DBG("Deasserting GPIO...");
  rc = gpio_pin_set_dt(&data->dev_config->interrupt, CROS_DSP_GPIO_OFF);
  if (rc != 0) {
    LOG_ERR("Failed to deassert 'ready' interrupt");
    return rc;
  }

  return 0;
}

static int command_dsp_init(const struct shell *shell, size_t argc,
                            char **argv) {
  shell_fprintf(shell, SHELL_NORMAL, "device %p initialized\n",
                (void *)dsp_service_dev);

  if (dsp_service_dev == NULL) {
    return 0;
  }

  const struct dsp_service_config *config = dsp_service_dev->config;
  struct dsp_service_data *data = dsp_service_dev->data;
  int rc;

  rc = i2c_target_unregister(config->bus, &data->target_config);
  shell_fprintf(shell, SHELL_NORMAL, "i2c_target_unregister (rc=%d)\n", rc);
  rc = i2c_target_register(config->bus, &data->target_config);
  shell_fprintf(shell, SHELL_NORMAL, "i2c_target_register (rc=%d)\n", rc);
  return 0;
}

SHELL_CMD_REGISTER(dsp_init, NULL, "Init the DSP service", command_dsp_init);

#define DSP_SERVICE_DEFINE(inst)                                               \
  static const struct dsp_service_config dsp_service_config_##inst = {         \
      .bus = DEVICE_DT_GET(DT_INST_BUS(inst)),                                 \
      .interrupt = GPIO_DT_SPEC_INST_GET(inst, int_gpios),                     \
  };                                                                           \
  static struct dsp_service_data dsp_service_data_##inst = {                   \
      .target_config =                                                         \
          {                                                                    \
              .address = DT_INST_REG_ADDR(inst),                               \
              .callbacks = &dsp_service_callbacks,                             \
          },                                                                   \
      .dev_config = &dsp_service_config_##inst,                                \
  };                                                                           \
  DEVICE_DT_INST_DEFINE(inst, dsp_service_init, NULL,                          \
                        &dsp_service_data_##inst, &dsp_service_config_##inst,  \
                        POST_KERNEL, 99, NULL);

DT_INST_FOREACH_STATUS_OKAY(DSP_SERVICE_DEFINE)