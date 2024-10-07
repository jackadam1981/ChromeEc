/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#define DT_DRV_COMPAT cros_dsp_client

#include <pb_decode.h>
#include <pb_encode.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>

#include "cros/dsp/client.h"
#include "proto/ec_dsp.pb.h"

LOG_MODULE_REGISTER(dsp_client, CONFIG_DSP_COMMS_LOG_LEVEL);

#define RESPONSE_BUFFER_SIZE 128

static const struct device* default_client_device =
    DEVICE_DT_GET_OR_NULL(DT_INST(0, DT_DRV_COMPAT));

int cbi_remote_get_board_info(enum cbi_data_tag tag,
                              uint8_t* buffer,
                              uint8_t* buffer_size) {
  int rc;
  cros_dsp_comms_CbiFlag flag;
  cros_dsp_comms_GetCbiFlagsResponse response;

  switch (tag) {
    case CBI_TAG_BOARD_VERSION:
      LOG_DBG("Getting BOARD_VERSION");
      flag = cros_dsp_comms_CbiFlag_VERSION;
      break;
    case CBI_TAG_OEM_ID:
      LOG_DBG("Getting OEM_ID");
      flag = cros_dsp_comms_CbiFlag_OEM;
      break;
    case CBI_TAG_SKU_ID:
      LOG_DBG("Getting SKU_ID");
      flag = cros_dsp_comms_CbiFlag_SKU;
      break;
    case CBI_TAG_MODEL_ID:
      LOG_DBG("Getting MODEL_ID");
      flag = cros_dsp_comms_CbiFlag_MODEL;
      break;
    case CBI_TAG_FW_CONFIG:
      LOG_DBG("Getting FW_CONFIG");
      flag = cros_dsp_comms_CbiFlag_FW_CONFIG;
      break;
    case CBI_TAG_PCB_SUPPLIER:
      LOG_DBG("Getting PCB_SUPPLIER");
      flag = cros_dsp_comms_CbiFlag_PCB_SUPPLIER;
      break;
    case CBI_TAG_SSFC:
      LOG_DBG("Getting SSFC");
      flag = cros_dsp_comms_CbiFlag_SSFC;
      break;
    case CBI_TAG_REWORK_ID:
      LOG_DBG("Getting REWORK_ID");
      flag = cros_dsp_comms_CbiFlag_REWORK;
      break;
    case CBI_TAG_FACTORY_CALIBRATION_DATA:
      LOG_DBG("Getting FACTORY_CALIBRATION_DATA");
      flag = cros_dsp_comms_CbiFlag_FACTORY_CALIBRATION_DATA;
      break;
    case CBI_TAG_DRAM_PART_NUM:
      LOG_DBG("Getting DRAM_PART_NUM");
      flag = cros_dsp_comms_CbiFlag_DRAM_PART_NUM;
      break;
    case CBI_TAG_OEM_NAME:
      LOG_DBG("Getting OEM_NAME");
      flag = cros_dsp_comms_CbiFlag_OEM_NAME;
      break;
    default:
      LOG_ERR("TAG not supported");
      return -EINVAL;
  }

  rc = dsp_client_get_cbi_flags(default_client_device, flag, &response);

  if (rc != 0) {
    LOG_ERR("Failed to get CBI flags");
    return rc;
  }

  memset(buffer, 0, *buffer_size);
  switch (response.which_flags) {
    case cros_dsp_comms_GetCbiFlagsResponse_flags_32_tag:
      if (*buffer_size < 4) {
        LOG_ERR("Not enough memory");
        return -ENOMEM;
      }
      memcpy(buffer, &response.flags.flags_32, 4);
      *buffer_size = 4;
      break;
    case cros_dsp_comms_GetCbiFlagsResponse_flags_64_tag:
      if (*buffer_size < 8) {
        LOG_ERR("Not enough memory");
        return -ENOMEM;
      }
      memcpy(buffer, &response.flags.flags_64, 8);
      *buffer_size = 8;
      break;
    case cros_dsp_comms_GetCbiFlagsResponse_flags_string_tag:
      if (*buffer_size < strlen(response.flags.flags_string)) {
        LOG_ERR("Not enough memory");
        return -ENOMEM;
      }
      memcpy(buffer,
             &response.flags.flags_string,
             strlen(response.flags.flags_string));
      *buffer_size = strlen(response.flags.flags_string);
      break;
    default:
      return -EINVAL;
  }
  return 0;
}

struct dsp_client_config {
  struct i2c_dt_spec i2c;
  struct gpio_dt_spec interrupt;
};

struct dsp_client_data {
  struct k_mutex mutex;
  struct k_event response_ready_event;
  const struct dsp_client_config* config;
  struct gpio_callback gpio_cb;
  int interrupt_config;
  cros_dsp_comms_EcService service;
  uint8_t request_buffer[cros_dsp_comms_EcService_size];
  uint8_t response_buffer[RESPONSE_BUFFER_SIZE];
};

#define GET_STATUS_RESPONSE_FLAG_SIZE \
  ARRAY_SIZE(((cros_dsp_comms_GetStatusResponse*)0)->flags)

static inline bool is_status_bit_set(
    uint8_t bit, const cros_dsp_comms_GetStatusResponse* status) {
  __ASSERT_NO_MSG(bit < GET_STATUS_RESPONSE_FLAG_SIZE * 8);
  return ((status->flags[bit / 8] >> (bit % 8)) & 0x1) == 1;
}

// TODO add dedicated function to reading the status

static int dsp_client_enable_interrupt(struct dsp_client_data* data,
                                       bool enable) {
  const struct dsp_client_config* config = data->config;
  int rc;

  if (!enable) {
    if (data->interrupt_config == GPIO_INT_EDGE_TO_ACTIVE) {
      // We're using an edge trigger, there's no need to actually disable things
      return 0;
    }
    LOG_INF("Disabling interrupts!");
    return gpio_pin_interrupt_configure_dt(&config->interrupt,
                                           GPIO_INT_DISABLE);
  }

  // Try level active if we haven't tried before or if previous attempt was a
  // level active
  if (data->interrupt_config == 0 ||
      data->interrupt_config == GPIO_INT_LEVEL_ACTIVE) {
    LOG_INF("Enabling level interrupts!");
    rc = gpio_pin_interrupt_configure_dt(&config->interrupt,
                                         GPIO_INT_LEVEL_ACTIVE);

    if (rc == 0) {
      data->interrupt_config = GPIO_INT_LEVEL_ACTIVE;
      return 0;
    }
  }

  LOG_WRN("GPIO driver does not support level interrupts");
  rc = gpio_pin_interrupt_configure_dt(&config->interrupt,
                                       GPIO_INT_EDGE_TO_ACTIVE);
  if (rc != 0) {
    LOG_ERR("Failed to configure interrupt");
    return rc;
  }

  data->interrupt_config = GPIO_INT_EDGE_TO_ACTIVE;

  // We can't detect levels, so poll the pin.
  if (gpio_pin_get_dt(&config->interrupt)) {
    // TODO get status
    LOG_DBG("GPIO is high");
  }

  return rc;
}

int dsp_client_get_cbi_flags(const struct device* dev,
                             cros_dsp_comms_CbiFlag flag,
                             cros_dsp_comms_GetCbiFlagsResponse* mem) {
  const struct dsp_client_config* cfg = dev->config;
  struct dsp_client_data* data = dev->data;
  cros_dsp_comms_EcService service = {
      .which_request = cros_dsp_comms_EcService_get_cbi_flags_tag,
      .request =
          {
              .get_cbi_flags =
                  {
                      .which = flag,
                  },
          },
  };
  int rc;

  k_mutex_lock(&data->mutex, K_FOREVER);
  pb_ostream_t stream = pb_ostream_from_buffer(data->request_buffer,
                                               cros_dsp_comms_EcService_size);
  bool encode_status =
      pb_encode(&stream, cros_dsp_comms_EcService_fields, &service);

  if (!encode_status) {
    LOG_ERR("Failed to encode request");
    k_mutex_unlock(&data->mutex);
    return -EINTR;
  }

  /* Blocking call */
  LOG_DBG("Writing to %p:%u", (void*)cfg->i2c.bus, cfg->i2c.addr);

  uint8_t status_buffer[cros_dsp_comms_GetStatusResponse_size + 4] = {0};
  cros_dsp_comms_GetStatusResponse status;

  /* Write the message */
  LOG_DBG("Writing %zu bytes", stream.bytes_written);
  rc = i2c_write_dt(&cfg->i2c, data->request_buffer, stream.bytes_written);

  if (rc != 0) {
    LOG_ERR("Failed to send request (%d)", rc);
    k_mutex_unlock(&data->mutex);
    return rc;
  }

  /* Wait for the EC to process the request */
  LOG_DBG("Waiting...");
  uint32_t events =
      k_event_wait(&data->response_ready_event,
                   1,
                   true,
                   K_MSEC(CONFIG_PLATFORM_EC_DSP_CLIENT_TIMEOUT_MS));

  if (events == 0) {
    LOG_ERR("Timed out waiting for response");
    if (gpio_pin_get_dt(&cfg->interrupt)) {
      // TODO get status
      LOG_DBG("GPIO is high");
    }
    k_mutex_unlock(&data->mutex);
    dsp_client_enable_interrupt(data, true);
    return -EAGAIN;
  }

  /* Read the expected response size */
  // TODO read GetStatusResponse, not just 4 bytes.
  LOG_DBG("Reading GetStatusResponse bytes");
  rc = i2c_read_dt(&cfg->i2c, status_buffer, ARRAY_SIZE(status_buffer));
  dsp_client_enable_interrupt(data, true);
  if (rc != 0) {
    LOG_ERR("Failed to send request (%d)", rc);
    k_mutex_unlock(&data->mutex);
    return rc;
  }
  printk("Read [");
  for (size_t i = 0; i < ARRAY_SIZE(status_buffer); ++i) {
    printk("0x%02x ", status_buffer[i]);
  }
  printk("]\n");

  pb_istream_t istream =
      pb_istream_from_buffer(status_buffer, ARRAY_SIZE(status_buffer));
  bool decode_status = pb_decode_delimited(
      &istream, cros_dsp_comms_GetStatusResponse_fields, &status);

  __ASSERT_NO_MSG(decode_status);
  __ASSERT_NO_MSG(status.response_length <= RESPONSE_BUFFER_SIZE);

  if (!is_status_bit_set(cros_dsp_comms_GetStatusResponse_Flag_RESPONSE_READY,
                         &status)) {
    LOG_ERR("Something went wrong, flags=[0x%02x, 0x%02x]",
            status.flags[0],
            status.flags[1]);
    k_mutex_unlock(&data->mutex);
    return -EINTR;
  }

  LOG_DBG("Expecting response of %u bytes", status.response_length);

  rc = i2c_read_dt(&cfg->i2c, data->response_buffer, status.response_length);
  if (rc != 0) {
    LOG_ERR("Failed to read response (%d)", rc);
    k_mutex_unlock(&data->mutex);
    return rc;
  }

  istream =
      pb_istream_from_buffer(data->response_buffer, status.response_length);
  decode_status =
      pb_decode(&istream, cros_dsp_comms_GetCbiFlagsResponse_fields, mem);

  if (!decode_status) {
    LOG_ERR("Failed to decode response");
    k_mutex_unlock(&data->mutex);
    return -EINTR;
  }

  k_mutex_unlock(&data->mutex);
  return rc;
}

static void dsp_client_gpio_callback(const struct device* port,
                                     struct gpio_callback* cb,
                                     uint32_t pin) {
  struct dsp_client_data* data =
      CONTAINER_OF(cb, struct dsp_client_data, gpio_cb);

  LOG_DBG("***** DSP SERVICE FIRED INTERRUPT *****");
  dsp_client_enable_interrupt(data, false);
  k_event_post(&data->response_ready_event, 1);
}

static int dsp_client_gpio_init(const struct device* dev) {
  const struct dsp_client_config* config = dev->config;
  struct dsp_client_data* data = dev->data;
  int rc;

  if (!gpio_is_ready_dt(&config->interrupt)) {
    LOG_ERR("GPIO port is not ready");
    return -EINVAL;
  }

  rc = gpio_pin_configure_dt(&config->interrupt, GPIO_INPUT);
  if (rc != 0) {
    LOG_ERR("Failed to configure gpio pin as input");
    return rc;
  }

  gpio_init_callback(
      &data->gpio_cb, dsp_client_gpio_callback, BIT(config->interrupt.pin));
  rc = gpio_add_callback(config->interrupt.port, &data->gpio_cb);
  if (rc != 0) {
    LOG_ERR("Failed to add callback to interrupt pin");
    return rc;
  }

  rc = dsp_client_enable_interrupt(data, true);
  if (rc != 0) {
    return rc;
  }

  if (data->interrupt_config == GPIO_INT_LEVEL_ACTIVE) {
    LOG_INF("Interrupt configured to LEVEL_ACTIVE");
  } else if (data->interrupt_config == GPIO_INT_EDGE_TO_ACTIVE) {
    LOG_INF("Interrupt configured to EDGE_TO_ACTIVE");
  } else {
    LOG_INF("Interrupt configured to %d", data->interrupt_config);
  }

  return 0;
}

static int dsp_client_init(const struct device* dev) {
  struct dsp_client_data* data = dev->data;

  k_mutex_init(&data->mutex);
  k_event_init(&data->response_ready_event);

  return dsp_client_gpio_init(dev);
}

#define DSP_CLIENT_DEFINE(inst)                                \
  static struct dsp_client_config dsp_client_config_##inst = { \
      .i2c = I2C_DT_SPEC_INST_GET(inst),                       \
      .interrupt = GPIO_DT_SPEC_INST_GET(inst, int_gpios),     \
  };                                                           \
  static struct dsp_client_data dsp_client_data_##inst = {     \
      .config = &dsp_client_config_##inst,                     \
  };                                                           \
  DEVICE_DT_INST_DEFINE(inst,                                  \
                        dsp_client_init,                       \
                        NULL,                                  \
                        &dsp_client_data_##inst,               \
                        &dsp_client_config_##inst,             \
                        POST_KERNEL,                           \
                        CONFIG_PLATFORM_EC_DSP_INIT_PRIORITY,  \
                        NULL);

DT_INST_FOREACH_STATUS_OKAY(DSP_CLIENT_DEFINE)
