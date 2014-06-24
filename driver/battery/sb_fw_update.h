/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Firmware Update Driver.
 */

#ifndef __EC_SB_FW_UPDATE__
#define __EC_SB_FW_UPDATE__

/**
 * Check if a Smart Battery Firmware Update is inprogress.
 *
 * @return 1 if YES, 0 if NO.
 */
int ec_sb_fw_update_is_inprogress(void);


/**
 * Check if a Smart Battery Firmware Update is protected.
 *
 * @return 1 if YES, 0 if NO.
 */
int ec_sb_fw_update_is_protect(void);

#endif
