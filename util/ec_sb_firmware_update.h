/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SB_FIRMWARE_H
#define __CROS_EC_SB_FIRMWARE_H

/**
 * Update Smart Battery Firmware
 *
 * @param fw_image_name  firmware image name
 *
 * @return 0 if success, negative if error.
 */
int ec_sb_firmware_update(const char *fw_image_name);

#endif
