/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_dsp_service

#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#include <cstddef>

#include "cros/dsp/internal/cros_transport.hh"
#include "cros/dsp/service/driver.hh"
#include "cros_board_info.h"
#include "lid_angle.h"
#include "proto/ec_dsp.pb.h"

static_assert(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
              "Must have exactly 1 cros,dsp-service");

static constexpr const struct i2c_target_callbacks dsp_service_callbacks = {
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

namespace cros::dsp::service {

Driver driver(
    {
        .address = DT_INST_REG_ADDR(0),
        .callbacks = &dsp_service_callbacks,
    },
    DEVICE_DT_GET(DT_INST_BUS(0)),
    GPIO_DT_SPEC_INST_GET(0, int_gpios));

}  // namespace cros::dsp::service

int init_driver() { return cros::dsp::service::driver.Init().ok() ? 0 : -1; }

SYS_INIT(init_driver, APPLICATION, 50);

LOG_MODULE_REGISTER(dsp_service, CONFIG_DSP_COMMS_LOG_LEVEL);

void dsp_service_handle_get_cbi_flags_request(struct k_work* work_item) {
  const cros_dsp_comms_GetCbiFlagsRequest* request =
      &(cros::dsp::service::driver.pending_service_request_.request
            .get_cbi_flags);
  enum cbi_data_tag tag;
  int rc = 0;
  uint8_t size = CROS_DSP_RESPONSE_BUFFER_SIZE;
  cros_dsp_comms_GetCbiFlagsResponse response =
      cros_dsp_comms_GetCbiFlagsResponse_init_zero;

  LOG_DBG("GOT: GetCbiFlagsRequest, which=%d", request->which);
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
      response.which_flags =
          cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag;
      tag = CBI_TAG_DRAM_PART_NUM;
      LOG_DBG("Fetching DRAM_PART_NUM");
      break;
    case cros_dsp_comms_CbiFlag_OEM_NAME:
      response.which_flags =
          cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag;
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
      rc = cbi_get_board_info(
          tag, reinterpret_cast<uint8_t*>(&response.flags.flags_32), &size);
      if (rc == 0 && size > 4) {
        LOG_ERR("Size is too big for buffer");
        rc = -EOVERFLOW;
      }
      LOG_DBG(
          "32 bit fetch rc(%d), value=(0x%08x)", rc, response.flags.flags_32);
      break;
    case cros_dsp_comms_GetCbiFlagsResponse_flags_64_tag:
      size = 8;
      rc = cbi_get_board_info(
          tag, reinterpret_cast<uint8_t*>(&response.flags.flags_64), &size);
      if (rc == 0 && size > 8) {
        LOG_ERR("Size is too big for buffer");
        rc = -EOVERFLOW;
      }
      break;
    case cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag:
      size = 80;
      rc = cbi_get_board_info(
          tag, reinterpret_cast<uint8_t*>(response.flags.flags_string), &size);
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
    if (!cros::dsp::service::driver.transport_.StageResponse(response).ok()) {
      LOG_ERR("Failed to stage response");
      rc = -EINVAL;
    }
  } else {
    LOG_ERR("Failed to read CBI value");
  }

  if (rc != 0) {
    cros::dsp::service::driver.transport_.SetStatusBit(
        cros_dsp_comms_GetStatusResponse_Flag_PROCESSING_ERROR);
  }
}

bool cros::dsp::service::Driver::AttemptToDecode() {
  // Try to decode
  pb_istream_t istream =
      pb_istream_from_buffer(request_buffer_, request_buffer_size_);
  return pb_decode(
      &istream, cros_dsp_comms_EcService_fields, &pending_service_request_);
}

bool cros::dsp::service::Driver::HandleDecodedRequest() {
  switch (pending_service_request_.which_request) {
    case cros_dsp_comms_EcService_notify_notebook_mode_change_tag:
      LOG_DBG("GOT: NotifyNotebookModeChangeRequest");
      if (IS_ENABLED(CONFIG_PLATFORM_EC_DSP_SERVICE_REMOTE_LID_ANGLE)) {
        switch (pending_service_request_.request.notify_notebook_mode_change
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
      k_work_submit(&get_cbi_flags_work_);
      return true;
    default:
      LOG_WRN("Unsupported request type");
      return false;
  }
}

pw::Status cros::dsp::service::Driver::Init() {
  k_work_init(&get_cbi_flags_work_, dsp_service_handle_get_cbi_flags_request);

  PW_ASSERT(k_sem_init(&data_processing_semaphore_, 1, 1) == 0);

  LOG_INF("Setting up target %s::0x%02x", bus_->name, target_cfg_.address);

  PW_ASSERT(i2c_target_register(bus_, &target_cfg_) == 0);

  PW_ASSERT(gpio_is_ready_dt(&interrupt_));

  PW_ASSERT(gpio_pin_configure_dt(&interrupt_, GPIO_OUTPUT) == 0);

  PW_ASSERT(gpio_pin_set_dt(&interrupt_, CROS_DSP_GPIO_OFF) == 0);

  transport_.SetNotifyClientCallback([this](bool has_data) {
    LOG_DBG("NotifyClientCallback(%d)", has_data);
    if (has_data) {
      k_sem_give(&this->data_processing_semaphore_);
    }
    gpio_pin_set_dt(&this->interrupt_,
                    has_data ? CROS_DSP_GPIO_ON : CROS_DSP_GPIO_OFF);
  });

  return pw::OkStatus();
}

#ifdef CONFIG_TEST
/*
 * There's a bug in the i2c_emul where every node in the i2c is required to have
 * a device* associated with it. So create a stub one for now until upstream is
 * patched up.
 */
int dsp_service_init(const struct device* dev) {
  ARG_UNUSED(dev);
  return 0;
}

DEVICE_DT_INST_DEFINE(
    0, dsp_service_init, NULL, NULL, NULL, POST_KERNEL, 99, NULL);
#endif