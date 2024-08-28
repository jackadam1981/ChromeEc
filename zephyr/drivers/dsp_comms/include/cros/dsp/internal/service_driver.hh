/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#include "cros/dsp/internal/cros_transport.hh"
#include "cros_transport.hh"
#include "proto/ec_ish.pb.h"

#define CROS_DSP_RESPONSE_BUFFER_SIZE 128

namespace cros::dsp::util {

template <typename ValueType, typename ClassType>
constexpr size_t OffsetOf(const ValueType ClassType::*member) {
  std::aligned_storage<sizeof(ClassType), alignof(ClassType)> obj_memory;
  ClassType* obj = reinterpret_cast<ClassType*>(&obj_memory);
  return reinterpret_cast<size_t>(&(obj->*member)) -
         reinterpret_cast<size_t>(obj);
}

template <typename ValueType, typename ClassType>
ClassType* ContainerOf(void* ptr, ValueType ClassType::*member) {
  return reinterpret_cast<ClassType*>(reinterpret_cast<char*>(ptr) -
                                      OffsetOf(member));
}

template <typename ValueType, typename ClassType>
const ClassType* ContainerOf(const void* ptr, ValueType ClassType::*member) {
  return reinterpret_cast<const ClassType*>(reinterpret_cast<const char*>(ptr) -
                                            OffsetOf(member));
}

}  // namespace cros::dsp::util

struct dsp_service_config {
  const struct device* bus;
  struct gpio_dt_spec interrupt;
};

constexpr const size_t kRequestBufferSize = cros_dsp_comms_EcService_size;

class dsp_service_data {
 public:
  dsp_service_data(struct i2c_target_config target_cfg,
                   const struct dsp_service_config* cfg)
      : target_config(target_cfg), dev_config(cfg) {}
  struct i2c_target_config target_config;
  const struct dsp_service_config* dev_config;

  uint8_t request_buffer[kRequestBufferSize];
  uint32_t request_buffer_size;
  cros_dsp_comms_EcService pending_service_request;

  struct k_work do_init_work;

  struct k_work get_cbi_flags_work;

  struct k_sem data_processing_semaphore;

  cros::dsp::service::CrosTransport transport_;

  struct {
    uint8_t has_status_pending : 1;
    uint8_t has_response_pending : 1;
    uint8_t _reserved : 6;
  } response_state;

  pw::ConstByteSpan pio_response_buffer_;
  uint8_t pio_response_buffer_position_;
};

#define CROS_DSP_GPIO_ON 1
#define CROS_DSP_GPIO_OFF 0

bool dsp_service_handle_decoded_request(dsp_service_data* data);

bool dsp_service_attempt_to_decode(dsp_service_data* data);

int dsp_service_read_requested(struct i2c_target_config* cfg, uint8_t* out);
int dsp_service_read_processed(struct i2c_target_config* cfg, uint8_t* out);
int dsp_service_write_requested(struct i2c_target_config* cfg);
int dsp_service_write_received(struct i2c_target_config* cfg, uint8_t in);
int dsp_service_stop(struct i2c_target_config* cfg);
void dsp_service_buf_write_received(struct i2c_target_config* cfg,
                                    uint8_t* ptr,
                                    uint32_t len);
int dsp_service_buf_read_requested(struct i2c_target_config* cfg,
                                   uint8_t** ptr,
                                   uint32_t* len);
