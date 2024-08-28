/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>
#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>

#include <cstring>

#include "cros/dsp/client.h"
#include "cros_board_info.h"

// DECLARE_FAKE_VALUE_FUNC(int, crec_flash_unprotected_read, int, int, char *);

namespace {
constexpr const struct device* kClient = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_dsp_client));
constexpr const struct device* kService = DEVICE_DT_GET_OR_NULL(DT_ALIAS(test_dsp_service));
static_assert(kClient != nullptr, "Missing alias 'test_dsp_client'");
static_assert(kService != nullptr, "Missing alias 'test_dsp_service'");

constexpr const struct gpio_dt_spec kClientInterruptSpec =
    GPIO_DT_SPEC_GET(DT_ALIAS(test_dsp_client), int_gpios);
constexpr const struct gpio_dt_spec kServiceInterruptSpec =
    GPIO_DT_SPEC_GET(DT_ALIAS(test_dsp_service), int_gpios);

constexpr const uint32_t kDefaultBoardVersion = 0x12345678;
constexpr const uint32_t kDefaultOemId = 0x23456789;
constexpr const uint32_t kDefaultSkuId = 0x3456789a;
constexpr const uint32_t kDefaultModelId = 0x456789ab;
constexpr const uint32_t kDefaultFwConfig = 0x56789abc;
constexpr const uint32_t kDefaultPcbSupplier = 0x6789abcd;
constexpr const uint32_t kDefaultSsfc = 0x789abcde;
constexpr const uint64_t kDefaultReworkId = 0x89abcdef01234567;
constexpr const uint32_t kDefaultFactoryCalibrationData = 0x9abcdef0;
constexpr const char* kDefaultDramPartNum = "DRAM-123";
constexpr const char* kDefaultOemName = "Google";

#define SUSPEND() k_usleep(1)

class DspComms : public ::testing::Test {
 protected:
  DspComms() {
    // We have to poke any CBI value to make sure that it was
    // initialized
    uint32_t ver;
    cbi_get_board_version(&ver);
  }
  void SetUp() override {
    // RESET_FAKE(crec_flash_unprotected_read);

    // crec_flash_unprotected_read_fake.custom_fake =
    // crec_flash_physical_read; FFF_RESET_HISTORY(); Set default
    // values
    SUSPEND();
    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_BOARD_VERSION,
                                 reinterpret_cast<const uint8_t*>(&kDefaultBoardVersion),
                                 static_cast<uint8_t>(sizeof(kDefaultBoardVersion))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_OEM_ID,
                                 reinterpret_cast<const uint8_t*>(&kDefaultOemId),
                                 static_cast<uint8_t>(sizeof(kDefaultOemId))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_SKU_ID,
                                 reinterpret_cast<const uint8_t*>(&kDefaultSkuId),
                                 static_cast<uint8_t>(sizeof(kDefaultSkuId))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_MODEL_ID,
                                 reinterpret_cast<const uint8_t*>(&kDefaultModelId),
                                 static_cast<uint8_t>(sizeof(kDefaultModelId))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_FW_CONFIG,
                                 reinterpret_cast<const uint8_t*>(&kDefaultFwConfig),
                                 static_cast<uint8_t>(sizeof(kDefaultFwConfig))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_PCB_SUPPLIER,
                                 reinterpret_cast<const uint8_t*>(&kDefaultPcbSupplier),
                                 static_cast<uint8_t>(sizeof(kDefaultPcbSupplier))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_SSFC,
                                 reinterpret_cast<const uint8_t*>(&kDefaultSsfc),
                                 static_cast<uint8_t>(sizeof(kDefaultSsfc))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_REWORK_ID,
                                 reinterpret_cast<const uint8_t*>(&kDefaultReworkId),
                                 static_cast<uint8_t>(sizeof(kDefaultReworkId))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_FACTORY_CALIBRATION_DATA,
                                 reinterpret_cast<const uint8_t*>(&kDefaultFactoryCalibrationData),
                                 static_cast<uint8_t>(sizeof(kDefaultFactoryCalibrationData))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_DRAM_PART_NUM,
                                 reinterpret_cast<const uint8_t*>(kDefaultDramPartNum),
                                 static_cast<uint8_t>(std::strlen(kDefaultDramPartNum))));

    ASSERT_EQ(0,
              cbi_set_board_info(CBI_TAG_OEM_NAME,
                                 reinterpret_cast<const uint8_t*>(kDefaultOemName),
                                 static_cast<uint8_t>(std::strlen(kDefaultOemName))));

    gpio_callbacks_.handler =
        [](const struct device* port, struct gpio_callback*, gpio_port_pins_t pins) {
          while (pins != 0) {
            int pin = __builtin_ctz(pins);
            int value = gpio_emul_output_get(port, pin);
            SUSPEND();
            gpio_emul_input_set(kClientInterruptSpec.port, kClientInterruptSpec.pin, value);

            SUSPEND();
            pins &= ~BIT(pin);
          }
        };
    gpio_callbacks_.pin_mask = BIT(kServiceInterruptSpec.pin);

    int current_service_pin_value =
        gpio_emul_output_get(kServiceInterruptSpec.port, kServiceInterruptSpec.pin);

    gpio_emul_input_set(
        kClientInterruptSpec.port, kClientInterruptSpec.pin, current_service_pin_value);
    ASSERT_EQ(0, gpio_add_callback_dt(&kServiceInterruptSpec, &gpio_callbacks_));
  }

  void TearDown() override {
    ASSERT_EQ(0, gpio_remove_callback_dt(&kServiceInterruptSpec, &gpio_callbacks_));
  }

  struct gpio_callback gpio_callbacks_ = {};
};

TEST_F(DspComms, ReadUnsupportedTag) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(-EINVAL,
            cbi_remote_get_board_info(CBI_TAG_COUNT, reinterpret_cast<uint8_t*>(&out), &size));
}

TEST_F(DspComms, FailToReadSmallBuffer) {
  uint8_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  EXPECT_EQ(
      -ENOMEM,
      cbi_remote_get_board_info(CBI_TAG_BOARD_VERSION, reinterpret_cast<uint8_t*>(&out), &size));

  EXPECT_EQ(-ENOMEM,
            cbi_remote_get_board_info(CBI_TAG_REWORK_ID, reinterpret_cast<uint8_t*>(&out), &size));

  EXPECT_EQ(
      -ENOMEM,
      cbi_remote_get_board_info(CBI_TAG_DRAM_PART_NUM, reinterpret_cast<uint8_t*>(&out), &size));
}

TEST_F(DspComms, ReadCbiVersion) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(
      0, cbi_remote_get_board_info(CBI_TAG_BOARD_VERSION, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultBoardVersion, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiOemId) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0, cbi_remote_get_board_info(CBI_TAG_OEM_ID, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultOemId, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiSkuId) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0, cbi_remote_get_board_info(CBI_TAG_SKU_ID, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultSkuId, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiModelId) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0,
            cbi_remote_get_board_info(CBI_TAG_MODEL_ID, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultModelId, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiFwConfig) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0,
            cbi_remote_get_board_info(CBI_TAG_FW_CONFIG, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultFwConfig, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiPcbSupplier) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(
      0, cbi_remote_get_board_info(CBI_TAG_PCB_SUPPLIER, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultPcbSupplier, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiSsfc) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0, cbi_remote_get_board_info(CBI_TAG_SSFC, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultSsfc, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiReworkId) {
  uint64_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0,
            cbi_remote_get_board_info(CBI_TAG_REWORK_ID, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultReworkId, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiFactoryCalibrationData) {
  uint32_t out;
  uint8_t size = static_cast<uint8_t>(sizeof(out));

  ASSERT_EQ(0,
            cbi_remote_get_board_info(
                CBI_TAG_FACTORY_CALIBRATION_DATA, reinterpret_cast<uint8_t*>(&out), &size));
  ASSERT_EQ(kDefaultFactoryCalibrationData, out);
  ASSERT_EQ(size, static_cast<uint8_t>(sizeof(out)));
}

TEST_F(DspComms, ReadCbiDramPartNum) {
  char out[80];
  uint8_t size = 80;

  ASSERT_EQ(
      0, cbi_remote_get_board_info(CBI_TAG_DRAM_PART_NUM, reinterpret_cast<uint8_t*>(out), &size));
  ASSERT_STREQ(kDefaultDramPartNum, out);
  ASSERT_LE(size, 80);
}

TEST_F(DspComms, ReadCbiOemName) {
  char out[80];
  uint8_t size = 80;

  ASSERT_EQ(0, cbi_remote_get_board_info(CBI_TAG_OEM_NAME, reinterpret_cast<uint8_t*>(out), &size));
  ASSERT_STREQ(kDefaultOemName, out);
  ASSERT_LE(size, 80);
}

}  // namespace
