/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SB_FIRMWARE_UPDATE_H__
#define __CROS_EC_SB_FIRMWARE_UPDATE_H__

struct smart_battery_fw_header {
	uint8_t signature[4]; /* "BTFW" */
	uint16_t hdr_version; /* 0x0100 */
	uint16_t pkg_version_major_minor;

	uint16_t vendor_id; /* 8,9 */
	uint16_t battery_type; /* A B */

	uint16_t fw_version; /* C D */
	uint16_t data_table_version; /* E F */
	uint32_t fw_binary_offset; /*0x10 0x11 0x12 0x13 */
	uint32_t fw_binary_size; /* 0x14 0x15 0x16 0x17 */
	uint8_t  checksum; /* 0x18 */
};

void print_battery_firmware_image_hdr(
	struct smart_battery_fw_header *hdr);
/**
 * Update Smart Battery Firmware
 *
 * @param fw_image_name  firmware image name
 *
 * @return 0 if success, negative if error.
 */
int ec_sb_firmware_update(const char *fw_image_name);

#endif
