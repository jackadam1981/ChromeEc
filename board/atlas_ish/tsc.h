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
#include "common.h"

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

struct __packed touch_frame {
	union {
		uint8_t raw[FRAME_PACKAGE_SIZE];
		struct __packed {
			struct frame_hdr hdr;
			uint8_t frame[FRAME_SIZE];
			uint8_t sense[SENSE_SIZE];
			uint8_t force[FORCE_SIZE];
		};
	};
};

struct __packed raw_touch_frame {
	union {
		uint8_t	mutual_buf[DATA_HEADER_SIZE + (FRAME_SIZE * 2)];
		struct __packed {
			struct data_hdr mutual_hdr;
			int16_t frame_raw[FRAME_SIZE];
		};
	};
	union {
		uint8_t self_buf[DATA_HEADER_SIZE + ((SENSE_SIZE
							+ FORCE_SIZE) * 2)];
		struct __packed {
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

/**
 * Touchpad initialize process.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int tsc_init(void);

/**
 * Get touchipad firmware id.
 *
 * @param fwuid Firmware unique ID return pointer.
 *
 * @return non-zero if error occurred.
 */
int tsc_get_fw_id(uint32_t *fwuid);

/**
 * Get touchipad firmware id.
 *
 * @param chksum Firmware check sum return pointer.
 *
 * @return non-zero if error occurred.
 */
int tsc_get_chksum(uint16_t *chksum);

/**
 * Set touchpad to sleep.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int tsc_sleep(void);

/**
 * Awake touchpad.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int tsc_wakeup(void);

/**
 * Read input report.
 *
 * @param tsc_report	Report data buffer pointer.
 * @param intp		Interrupt flag to indicate if current
 *			read action is triggered by interrupt.
 *
 * @return non-zero if error occurred.
 */
int tsc_read_report(union hid_report *tsc_report, int intp);

/**
 * Read heatmap.
 *
 * @param frame	Heatmap data buffer pointer.
 *
 * @return non-zero if error occurred.
 */
int tsc_read_frame(struct touch_frame *frame);

/**
 * Send ACK to touchpad.
 *
 * @param None
 *
 * @return non-zero if error occurred.
 */
int tsc_send_ack(void);

/**
 * Get snapshot touch sensing image from touchpad.
 *
 * @param fidx	Image type index.
 *
 * @return non-zero if error occurred.
 */
int tsc_snapshot_frame(enum snapshot_frame_idx fidx);

#endif /* __CROS_EC_TSC_H */
