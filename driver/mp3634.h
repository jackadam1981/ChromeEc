/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PMIC_MP3634_H
#define __CROS_EC_PMIC_MP3634_H

#define MP3634_PAGE 0xEF
#define MP3634_STORE_USER_ALL 0x15
#define MP3634_RESTORE_USER_ALL 0x16
#define MP3634_SET_FW_VER_LSB 0x70
#define MP3634_SET_FW_VER_MSB 0x71
#define MP3634_CRC_PAGE_GROUP_1 0x7e
#define MP3634_REVISION_ID 0x95
#define MP3634_I2C_FW_VERSION 0x96
#define MP3634_PRODUCT_ID 0x98
#define MP3634_NVM_PROGRAM_STATUS 0xec
#define MP3634_ENTER_CONF_MODE 0xf1
#define MP3634_UNLOCK_NVM 0xfc

struct mp3634_reg_val {
	uint8_t reg;
	uint8_t val;
};

int mp3634_tune(const struct mp3634_reg_val *page0, int count0,
		const struct mp3634_reg_val *page1, int count1);

#endif /* __CROS_EC_PMIC_MP3634_H */
