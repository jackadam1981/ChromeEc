/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Smart battery Firmware Update driver.
 * Ref: Common Smart Battery System Interface Specification v8.0.
 *
 * cmd.0x35, Write Word
 *   0x1000: Prepare to Update
 *   0x2000: End of Update
 *   0xF000: Update Firmware
 *
 * cmd.0x35, Read Word
 *   Firmware Update Status
 *
 * cmd.0x36 Write Block
 *   Send 32 byte firmware image
 *
 * cmd.0x37 Read Word
 *   Get Battery Information
 *   sequence:=b1,b0,b3,b2,b5,b5,b7,b6
 *
 * Command Sequence for Battery FW Update
 *
 *  0. cmd.0x35.read
 *  1. cmd.0x37.read
 *  2. cmd.0x35.write.0x1000
 *  3. cmd.0x35.read.status (optional)
 *  4. cmd.0x35.write.0xF000
 *  5. cmd.0x35.read.status
 *     if bit8-0, go to step 2
 *  6. cmd.0x36.write.32byte
 *  7. cmd.0x35.read.status
 *     if FEC.b13=1, go to step 6
 *     if fatal.b12=1, go to step 2
 *     if b11,b10,b9,b2,b1,b0; go to step 1
 *     if b5,b3; go to step 8
 *    (repeat 6,7)
 *  8. cmd.0x36.write.0x2000
 *  9. cmd.0x35.read.status
 */

#ifndef __EC_SB_FW_UPDATE__
#define __EC_SB_FW_UPDATE__

#define SB_FW_UPDATE_CMD_WRITE_WORD  0x35
#define SB_FW_UPDATE_CMD_WRITE_WORD_PREPARE  0x1000
#define SB_FW_UPDATE_CMD_WRITE_WORD_END      0x2000
#define SB_FW_UPDATE_CMD_WRITE_WORD_UPDATE   0xF000

#define SB_FW_UPDATE_CMD_READ_STATUS 0x35

#define SB_FW_UPDATE_CMD_WRITE_BLOCK 0x36

#define SB_FW_UPDATE_CMD_READ_INFO   0x37

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
