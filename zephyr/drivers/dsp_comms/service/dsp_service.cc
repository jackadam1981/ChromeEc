/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_dsp_service

#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#include <cstddef>

#include "cros/dsp/internal/cros_transport.hh"
#include "cros/dsp/internal/service_driver.hh"
#include "cros_board_info.h"
#include "lid_angle.h"
#include "proto/ec_ish.pb.h"

LOG_MODULE_REGISTER(dsp_service, CONFIG_DSP_COMMS_LOG_LEVEL);

static void dsp_service_do_init(struct k_work* work_item) {
  dsp_service_data* data = cros::dsp::util::ContainerOf(work_item, &dsp_service_data::do_init_work);
  const auto* config = data->dev_config;

  LOG_INF("Setting up target %s::0x%02x", config->bus->name, data->target_config.address);

  if (int rc = i2c_target_register(config->bus, &data->target_config); rc != 0) {
    LOG_ERR("Failed to configure bus as target (%d)", rc);
    return;
  }

  if (!gpio_is_ready_dt(&config->interrupt)) {
    LOG_ERR("GPIO is not ready");
    return;
  }

  if (int rc = gpio_pin_configure_dt(&config->interrupt, GPIO_OUTPUT); rc != 0) {
    LOG_ERR("Failed to configure interrupt as output (%d)", rc);
    return;
  }

  // Deassert GPIO
  LOG_DBG("Deasserting GPIO...");
  LOG_DBG("Using spec at %p / %p", data->dev_config, config);

  if (int rc = gpio_pin_set_dt(&data->dev_config->interrupt, CROS_DSP_GPIO_OFF); rc != 0) {
    LOG_ERR("Failed to deassert 'ready' interrupt");
    return;
  }

  LOG_INF("Init data=%p", data);
  data->transport_.SetNotifyClientCallback([data](bool has_data) {
    LOG_INF("NotifyClientCallback(%d), data=%p", has_data, data);
    if (has_data) {
      k_sem_give(&data->data_processing_semaphore);
    }
    gpio_pin_set_dt(&data->dev_config->interrupt, has_data ? CROS_DSP_GPIO_ON : CROS_DSP_GPIO_OFF);
  });
}

static void dsp_service_handle_get_cbi_flags_request(struct k_work* work_item) {
  // unsigned int lock = irq_lock();
  dsp_service_data* data =
      cros::dsp::util::ContainerOf(work_item, &dsp_service_data::get_cbi_flags_work);
  const cros_dsp_comms_GetCbiFlagsRequest* request =
      &(data->pending_service_request.request.get_cbi_flags);
  enum cbi_data_tag tag;
  int rc = 0;
  uint8_t size = CROS_DSP_RESPONSE_BUFFER_SIZE;
  cros_dsp_comms_GetCbiFlagsResponse response = cros_dsp_comms_GetCbiFlagsResponse_init_zero;

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
      rc = cbi_get_board_info(tag, reinterpret_cast<uint8_t*>(&response.flags.flags_32), &size);
      if (rc == 0 && size > 4) {
        LOG_ERR("Size is too big for buffer");
        rc = -EOVERFLOW;
      }
      LOG_DBG("32 bit fetch rc(%d), value=(0x%08x)", rc, response.flags.flags_32);
      break;
    case cros_dsp_comms_GetCbiFlagsResponse_flags_64_tag:
      size = 8;
      rc = cbi_get_board_info(tag, reinterpret_cast<uint8_t*>(&response.flags.flags_64), &size);
      if (rc == 0 && size > 8) {
        LOG_ERR("Size is too big for buffer");
        rc = -EOVERFLOW;
      }
      break;
    case cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag:
      size = 80;
      rc = cbi_get_board_info(tag, reinterpret_cast<uint8_t*>(response.flags.flags_string), &size);
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
    if (!data->transport_.StageResponse(response).ok()) {
      LOG_ERR("Failed to stage response");
      rc = -EINVAL;
    }
  } else {
    LOG_ERR("Failed to read CBI value");
  }

  if (rc != 0) {
    data->transport_.SetStatusBit(cros_dsp_comms_GetStatusResponse_Flag_PROCESSING_ERROR);
  }
}

bool dsp_service_attempt_to_decode(dsp_service_data* data) {
  // Try to decode
  pb_istream_t istream = pb_istream_from_buffer(data->request_buffer, data->request_buffer_size);
  return pb_decode(&istream, cros_dsp_comms_EcService_fields, &data->pending_service_request);
}

bool dsp_service_handle_decoded_request(dsp_service_data* data) {
  switch (data->pending_service_request.which_request) {
    case cros_dsp_comms_EcService_notify_notebook_mode_change_tag:
      LOG_DBG("GOT: NotifyNotebookModeChangeRequest");
      if (IS_ENABLED(CONFIG_PLATFORM_EC_DSP_SERVICE_REMOTE_LID_ANGLE)) {
        switch (data->pending_service_request.request.notify_notebook_mode_change.new_mode) {
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

static int dsp_service_init(const struct device* dev) {
  const auto* config = reinterpret_cast<const struct dsp_service_config*>(dev->config);
  auto* data = reinterpret_cast<dsp_service_data*>(dev->data);
  int rc = 0;

  LOG_ERR("Initializing DSP (%p) service with callbacks @%p", data, (void*)&dsp_service_callbacks);

  data->dev_config = config;

  k_work_init(&data->do_init_work, dsp_service_do_init);

  k_work_init(&data->get_cbi_flags_work, dsp_service_handle_get_cbi_flags_request);

  rc = k_sem_init(&data->data_processing_semaphore, 1, 1);
  if (rc != 0) {
    LOG_ERR("Failed to init semaphore (%d)", rc);
    return rc;
  }

  // TODO(https://github.com/zephyrproject-rtos/zephyr/issues/77538)
  // Initialization must take place after device initialization because
  // constructors run after POST_KERNEL and devices cannot run in the
  // APPLICATION domain.
  k_work_submit(&data->do_init_work);

  return 0;
}

#define DSP_SERVICE_DEFINE(inst)                                       \
  static const struct dsp_service_config dsp_service_config_##inst = { \
      .bus = DEVICE_DT_GET(DT_INST_BUS(inst)),                         \
      .interrupt = GPIO_DT_SPEC_INST_GET(inst, int_gpios),             \
  };                                                                   \
  static dsp_service_data dsp_service_data_##inst(                     \
      {                                                                \
          .address = DT_INST_REG_ADDR(inst),                           \
          .callbacks = &dsp_service_callbacks,                         \
      },                                                               \
      &dsp_service_config_##inst);                                     \
  DEVICE_DT_INST_DEFINE(inst,                                          \
                        dsp_service_init,                              \
                        NULL,                                          \
                        &dsp_service_data_##inst,                      \
                        &dsp_service_config_##inst,                    \
                        POST_KERNEL,                                   \
                        99,                                            \
                        NULL);

DT_INST_FOREACH_STATUS_OKAY(DSP_SERVICE_DEFINE)
