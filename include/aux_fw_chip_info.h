/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AUX FW chip configuration and host command handler */

#ifndef __CROS_AUX_FW_CHIP_INFO_H
#define __CROS_AUX_FW_CHIP_INFO_H

#include "common.h"
#include "ec_commands.h"

/**
 * Get chips requiring FW update
 *
 * @param chip_info	Out parameter to hold the return chip info.
 * @return		-ve error codes on error, number of chips
 *			requiring FW update on success.
 */
int board_get_aux_fw_chip_list(const struct aux_fw_chip_info **chip_info)
						__attribute__((weak));

#endif /* __CROS_AUX_FW_CHIP_INFO_H */
