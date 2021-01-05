/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault host info interface implementation
 */


#include "memfault/core/platform/device_info.h"
#include "memfault/core/debug_log.h"

#include <printf.h>
#include <string.h>

#include "system.h"
#include "cros_board_info.h"
#include "ec_version.h"

static const char *prv_get_device_serial(void) {
  // Return empty serial so it can be filled in by host
  return "";
}

static const char *prv_get_hardware_version(void) {
  uint32_t board_version = 0;
  uint32_t sku_id = 0;
  static char hardware_version[32];

  cbi_get_board_version(&board_version);
  cbi_get_sku_id(&sku_id);

  snprintf(hardware_version, sizeof(hardware_version), "%s_%08x_%d", STRINGIFY(BOARD), sku_id, board_version);
  return hardware_version;
}

static const char *prv_get_software_version(void) {
  return VERSION ":" STRINGIFY(TIMESTAMP);
}

static const char *prv_get_software_type(void) {
  const char * image_type;
  static char software_type[32];

  image_type = system_get_image_copy_string();
  snprintf(software_type, sizeof(software_type), "%s_EC_%s", STRINGIFY(BOARD), image_type);

  return software_type;
}

void memfault_platform_get_device_info(struct MemfaultDeviceInfo *info) {
  *info = (struct MemfaultDeviceInfo) {
    .device_serial = prv_get_device_serial(),
    .hardware_version = prv_get_hardware_version(),
    .software_version = prv_get_software_version(),
    .software_type = prv_get_software_type(),
  };
}
