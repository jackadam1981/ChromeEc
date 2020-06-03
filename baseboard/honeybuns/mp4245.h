/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* MPS MP4245 Buck-Boost converter driver definitions */

/* I2C addresses */
#define MP4245_I2C_ADDR_0_FLAGS 0x61  /* R1 -> GND */
#define MP4245_I2C_ADDR_1_FLAGS 0x62  /* R1 -> 15.0k */
#define MP4245_I2C_ADDR_2_FLAGS 0x63  /* R1 -> 25.5k */
#define MP4245_I2C_ADDR_3_FLAGS 0x64  /* R1 -> 35.7k */
#define MP4245_I2C_ADDR_4_FLAGS 0x65  /* R1 -> 45.3k */
#define MP4245_I2C_ADDR_5_FLAGS 0x66  /* R1 -> 56.0k */
#define MP4245_I2C_ADDR_6_FLAGS 0x67  /* R1 -> VCC */


/* MP4245 CMD Offsets */
#define MP4245_CMD_OPERATION         0x01
#define MP4245_CMD_CLEAR_FAULTS      0x03
#define MP4245_CMD_WRITE_PROTECT     0x10
#define MP4245_CMD_STORE_USER_ALL    0x15
#define MP4245_CMD_RESTORE_USER_ALL  0x16
#define MP4245_CMD_VOUT_MODE         0x20
#define MP4245_CMD_VOUT_COMMAND      0x21
#define MP4245_CMD_VOUT_SCALE_LOOP   0x29
#define MP4245_CMD_STATUS_BYTE       0x78
#define MP4245_CMD_STATUS_WORD       0x79
#define MP4245_CMD_STATUS_VOUT       0x7A
#define MP4245_CMD_STATUS_INPUT      0x7C
#define MP4245_CMD_STATUS_TEMP       0x7D
#define MP4245_CMD_STATUS_CML        0x7E
#define MP4245_CMD_READ_VIN          0x88
#define MP4245_CMD_READ_VOUT         0x8B
#define MP4245_CMD_READ_IOUT         0x8C
#define MP4245_CMD_READ_TEMP         0x8D
#define MP4245_CMD_MFR_MODE_CTRL     0xD0
#define MP4245_CMD_MFR_CURRENT_LIM   0xD1
#define MP4245_CMD_MFR_LINE_DROP     0xD2
#define MP4245_CMD_MFR_OT_FAULT_LIM  0xD3
#define MP4245_CMD_MFR_OT_WARN_LIM   0xD4
#define MP4245_CMD_MFR_CRC_ERROR     0xD5
#define MP4245_CMD_MFF_MTP_CFG_CODE  0xD6
#define MP4245_CMD_MFR_MTP_REV_NUM   0xD7
#define MP4245_CMD_MFR_STATUS_MASK   0xD8

#define MP4245_CMD_OPERATION_ON      BIT(7)

/*
 * For a desired voltage output Vdes, Vout = Vdes * 1024. In other words, there
 * are 10fractional and 6 integer bits. Vdes is stored in in mV so this scaling
 * to mV must also be accounted for.
 *
 * VOUT_COMMAND = (Vdes (mV) * 1024 / 1000) / 1024
 */
#define MP4245_VOUT_ONE_VOLT         BIT(10)
#define MP4245_VOUT_FROM_MV          (MP4245_VOUT_ONE_VOLT * MP4245_VOUT_ONE_VOLT / 1000)
#define MP4245_ILIM_STEP_MA          50


int mp4245_set_voltage_out(int desired_mv);
int mp4245_set_current_lim(int desired_ma);
int mp4245_votlage_out_enable(int enable);
void mp4245_dump_reg(void);



