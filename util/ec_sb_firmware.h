/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SB_FIRMWARE_H
#define __CROS_EC_SB_FIRMWARE_H

/**
 * Write Smart Battery Firmwre
 *
 * @param buf		Source buffer
 * @param size		Number of bytes to write
 *
 * @return 0 if success, negative if error.
 */
int ec_sb_firmware_write(const uint8_t *buf, int size);

#endif
