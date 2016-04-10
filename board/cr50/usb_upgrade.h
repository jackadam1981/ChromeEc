/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_UPGRADE_H
#define __CROS_EC_USB_UPGRADE_H

#define UNOFFICIAL_USB_SUBCLASS_GOOGLE_CR50   0x53


/* Commands from host */
#define UPGRADE_START         0xC0DEF00D
#define UPGRADE_DONE          0xB007AB1E

/* Replies from device */
struct usb_upgrade_reply {
	uint32_t status;
	uint32_t offset;
};

/* Status values */
#define UPGRADE_EXPECT_RW_A   0xFEED000A
#define UPGRADE_EXPECT_RW_B   0xFEED000B
#define UPGRADE_FAILURE       0x0BADC0DE
#define UPGRADE_MEH           0x00000000

/* Helper macros */
#define IS_EXPECT_RW(status) (((status) & 0xFFFFFFF0) == 0xFEED0000)
#define WHICH_IMAGE(status)  ((status) & 0x0000000F)

/* Offsets into full RO+RW image file */
#define FW_IMAGE_SIZE 0x00080000
#define RO_SIZE       0x00004000
#define RW_A_OFFSET   0x00004000
#define RW_SIZE       0x0003c000
#define NV_SIZE       0x00004000
#define RW_B_OFFSET   0x00044000

#endif	/* __CROS_EC_USB_UPGRADE_H */
