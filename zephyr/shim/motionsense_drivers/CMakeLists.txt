# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(CONFIG_DT_HAS_CROS_EC_BMI160_ACCEL_ENABLED AND CONFIG_BMI160)
  zephyr_library_sources(src/bmi160.c)
endif()
