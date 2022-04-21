/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _DOJO_CBI_FW_CONFIG__H_
#define _DOJO_CBI_FW_CONFIG__H_

/****************************************************************************
 * Dojo CBI FW Configuration
 */

/*
 * Keyboard backlight (1 bit)
 */
enum fw_config_kblight_type {
	KB_BL_ABSENT = 0,
	KB_BL_PRESENT = 1,
};
#define FW_CONFIG_KB_BL_OFFSET			0
#define FW_CONFIG_KB_BL_MASK			GENMASK(0, 0)

enum fw_config_kblight_type get_cbi_fw_config_kblight(void);

#endif /* _DOJO_CBI_FW_CONFIG__H_ */
