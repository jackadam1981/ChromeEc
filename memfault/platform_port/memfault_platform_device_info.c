/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault host info interface implementation
 */


#include "memfault/core/platform/device_info.h"
#include "memfault/core/math.h"

#include <printf.h>
#include <string.h>

#include "system.h"

static char *prv_get_device_serial(void) {
  return "1234567";
}

static char *prv_get_hardware_version(void) {
  static char board_version[4];
  snprintf(board_version, sizeof(board_version), "%d", system_get_board_version());
  return board_version;
}

static char *prv_get_software_version(void) {

  return system_get_version(system_get_image_copy());
}

static char *prv_get_software_type(void) {
  return system_get_image_copy_string();
}

void memfault_platform_get_device_info(struct MemfaultDeviceInfo *info) {
  *info = (struct MemfaultDeviceInfo) {
    .device_serial = prv_get_device_serial(),
    .hardware_version = prv_get_hardware_version(),
    .software_version = prv_get_software_version(),
    .software_type = prv_get_software_type(),
  };
}
