/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FIRMWARE_H
#define __CROS_EC_FIRMWARE_H


/**
 * list all firmware
 *
 * @return none
 */
int ec_firmware_list(void);

/**
 * Update Firmware
 *
 * @param fw_id    firmware id
 * @param fw_image_name  firmware image name
 *
 * @return 0 if success, negative if error.
 */
int ec_firmware_update(uint32_t fw_id, const char *fw_image_name);

#endif
