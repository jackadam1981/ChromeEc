/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Routines for communicating with TSC */

#ifndef __CROS_EC_TSC_H
#define __CROS_EC_TSC_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_hid_master.h"

#define ROWS		26
#define COLS		15

/* For heatmap frame we add one more byte to show the index in TSC. */
#define FRAME_HEADER_SIZE	4
/* Normal HID data header include 2 bytes of length and 1 byte RID. */
#define DATA_HEADER_SIZE	3

#define FRAME_SIZE	(ROWS * COLS)
#define SENSE_SIZE      (COLS)
#define FORCE_SIZE	(ROWS)

#define N_FRAME_TYPES	3

#define FRAME_PACKAGE_SIZE	(FRAME_HEADER_SIZE + FRAME_SIZE + FORCE_SIZE \
					+ SENSE_SIZE)
/* Snapshot Information */
enum snapshot_frame_idx {
	SNAPSHOT_FIDX_DV		= 0,
	SNAPSHOT_FIDX_BASE		= 1,
	SNAPSHOT_FIDX_RAW		= 2,
	SNAPSHOT_FIDX_OPENSHORT		= 3,
	SNAPSHOT_FIDX_MAX		= 4,
};

enum tsc_error_list {
	TSC_OK				= 0,
	TSC_PARAM_ERR			= 1,
	TSC_RESET_FAIL			= 2,
	TSC_RD_INPUTRPT_FAIL		= 3,
	TSC_INTP_ON_TIMEOUT		= 4,
	TSC_INTP_OFF_TIMEOUT		= 5,
	TSC_MODE_PTP_FAIL		= 6,
	TSC_MODE_MOUSE_FAIL		= 7,
	TSC_SLEEP_FAIL			= 8,
	TSC_WAKEUP_FAIL			= 9,
	TSC_GETINPUT_FAIL		= 10,
	TSC_GETHEATMAP_FAIL		= 11,
	TSC_ACK_FAIL			= 14,
	TSC_GETCHKSUM_FAIL		= 15,
	TSC_GETFWUID_FAIL		= 16,
	TSC_TMODE_START_FAIL		= 17,
	TSC_TMODE_SETCHK_FAIL		= 18,
	TSC_TMODE_GETCHK_FAIL		= 19,
	TSC_TMODE_STATCHK_FAIL		= 20,
	TSC_TMODE_GETDAT_FAIL		= 21,
	TSC_TMODE_STOP_FAIL		= 22,
};

struct __attribute__((packed)) touch_frame {
	union {
		uint8_t raw[FRAME_PACKAGE_SIZE];
		struct __attribute__((packed)) {
			struct frame_hdr hdr;
			uint8_t frame[FRAME_SIZE];
			uint8_t sense[SENSE_SIZE];
			uint8_t force[FORCE_SIZE];
		};
	};
};

struct __attribute__((packed)) raw_touch_frame {
	union {
		uint8_t	mutual_buf[DATA_HEADER_SIZE + (FRAME_SIZE * 2)];
		struct __attribute__((packed)) {
			struct data_hdr mutual_hdr;
			int16_t frame_raw[FRAME_SIZE];
		};
	};
	union {
		uint8_t self_buf[DATA_HEADER_SIZE + ((SENSE_SIZE + FORCE_SIZE) * 2)];
		struct __attribute__((packed)) {
			struct data_hdr self_hdr;
			int16_t sense_raw[SENSE_SIZE];
			int16_t force_raw[FORCE_SIZE];
		};
	};
};

/* Extern data */
extern struct raw_touch_frame raw_frame;
extern const uint32_t snapshot_ntype_lst[SNAPSHOT_FIDX_MAX];
extern const uint8_t **snapshot_src_map[SNAPSHOT_FIDX_MAX];
extern const int *snapshot_siz_map[SNAPSHOT_FIDX_MAX];

/* APIs */
int tsc_init(void);
int tsc_get_fw_id(void);
int tsc_sleep(void);
int tsc_wakeup(void);
int tsc_read_report(union hid_report *tsc_report, int intp);
int tsc_read_frame(struct touch_frame* frame);
int tsc_send_ack(void);
int tsc_self_test(uint32_t *fwuid, uint16_t *chksum);
int tsc_snapshot_frame(enum snapshot_frame_idx fidx);

#endif /* __CROS_EC_TSC_H */
