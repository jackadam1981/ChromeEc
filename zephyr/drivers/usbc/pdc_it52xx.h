/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Internal header with RTS54xx interface constants and types */

#ifndef __CROS_EC_PDC_RTS54XX_H
#define __CROS_EC_PDC_RTS54XX_H

#include "drivers/pdc.h"

#include <string.h>

/**
 * @brief RTS54XX I2C block read command
 */
//define RTS54XX_BLOCK_READ_CMD 0x80
/* CCI byte count and 4bytes data  */
#define PDC_MAX_CCI_LENGTH 0x05

/**
 * @brief Offsets of data fields in the GET_IC_STATUS response
 *
 * These are based on the Realtek spec version 3.3.25.
 *
 * "Data Byte 0" is the first byte after "Byte Count" and is available
 * at .rd_buf[1].
 */
#define RTS54XX_GET_IC_STATUS_RUNNING_FLASH_CODE 1
#define RTS54XX_GET_IC_STATUS_FWVER_MAJOR_OFFSET 4
#define RTS54XX_GET_IC_STATUS_FWVER_MINOR_OFFSET 5
#define RTS54XX_GET_IC_STATUS_FWVER_PATCH_OFFSET 6
#define RTS54XX_GET_IC_STATUS_VID_L 10
#define RTS54XX_GET_IC_STATUS_VID_H 11
#define RTS54XX_GET_IC_STATUS_PID_L 12
#define RTS54XX_GET_IC_STATUS_PID_H 13
#define RTS54XX_GET_IC_STATUS_RUNNING_FLASH_BANK 15
#define RTS54XX_GET_IC_STATUS_PD_REV_MAJOR_OFFSET 23
#define RTS54XX_GET_IC_STATUS_PD_REV_MINOR_OFFSET 24
#define RTS54XX_GET_IC_STATUS_PD_VER_MAJOR_OFFSET 25
#define RTS54XX_GET_IC_STATUS_PD_VER_MINOR_OFFSET 26
#define RTS54XX_GET_IC_STATUS_PROG_NAME_STR 27
#define RTS54XX_GET_IC_STATUS_PROG_NAME_STR_LEN 12
#define RTS54XX_GET_IC_STATUS_SBU_MUX_MODE_OFFSET 39

static inline void
rts54xx_unpack_get_ic_status_response(uint8_t *rx_buf, struct pdc_info_t *info)
{
	/* Realtek Is running flash code: Data Byte0 */
	info->is_running_flash_code =
		rx_buf[RTS54XX_GET_IC_STATUS_RUNNING_FLASH_CODE];

	/* Realtek FW main version: Data Byte3..5 */
	info->fw_version =
		rx_buf[RTS54XX_GET_IC_STATUS_FWVER_MAJOR_OFFSET] << 16 |
		rx_buf[RTS54XX_GET_IC_STATUS_FWVER_MINOR_OFFSET] << 8 |
		rx_buf[RTS54XX_GET_IC_STATUS_FWVER_PATCH_OFFSET];

	/* Realtek VID: Data Byte9..10 (little-endian) */
	info->vid = rx_buf[RTS54XX_GET_IC_STATUS_VID_H] << 8 |
		    rx_buf[RTS54XX_GET_IC_STATUS_VID_L];

	/* Realtek PID: Data Byte11..12 (little-endian) */
	info->pid = rx_buf[RTS54XX_GET_IC_STATUS_PID_H] << 8 |
		    rx_buf[RTS54XX_GET_IC_STATUS_PID_L];

	/* Realtek Running flash bank offset: Data Byte14 */
	info->running_in_flash_bank =
		rx_buf[RTS54XX_GET_IC_STATUS_RUNNING_FLASH_BANK];

	/* Realtek PD Revision: Data Byte22..23 (big-endian) */
	info->pd_revision = rx_buf[RTS54XX_GET_IC_STATUS_PD_REV_MAJOR_OFFSET]
				    << 8 |
			    rx_buf[RTS54XX_GET_IC_STATUS_PD_REV_MINOR_OFFSET];

	/* Realtek PD Version: Data Byte24..25 (big-endian) */
	info->pd_version = rx_buf[RTS54XX_GET_IC_STATUS_PD_VER_MAJOR_OFFSET]
				   << 8 |
			   rx_buf[RTS54XX_GET_IC_STATUS_PD_VER_MINOR_OFFSET];
}

/* FW project name length should not exceed the max length supported in struct
 * pdc_info_t
 */
BUILD_ASSERT(RTS54XX_GET_IC_STATUS_PROG_NAME_STR_LEN <=
	     (sizeof(((struct pdc_info_t *)0)->project_name) - 1));

#define RTS54XX_GET_IC_STATUS_SBU_MUX_MODE_NORMAL 0
#define RTS54XX_GET_IC_STATUS_SBU_MUX_MODE_FORCE_DBG 1

/**
 * @brief PDC Command states
 */
enum cmd_sts_t {
	/** Command has not been started */
	CMD_RESET_COMPLETE = 0x08,
	CMD_ACK_COMPLETE = 0x20,
	CMD_BUSY = 0x10,
	/** Command has completed */
	CMD_DONE = 0x80,
	/** Command has been started but has not completed */
	//CMD_DEFERRED = 0x10,
	/** Command completed with error. Send GET_ERROR_STATUS for details */
	CMD_ERROR = 0x40
};

/**
 * @brief Ping Status of the PDC
 */
union ping_status_t {
	/* rtk cci only reply one byte */
	//struct {
	//	/** Command status */
	//	uint8_t cmd_sts : 2;
	//	/** Length of data read to read */
	//	uint8_t data_len : 6;
	//};
	//uint8_t raw_value;

	/* ite(spec) cci reply five bytes ByteCount - xx xx(MSGIN len) xx xx(status: ... /busy/ACK/error/CC) */
	struct {
		/** Byte count of CCI Command */
		uint8_t cci_byte_cnt;
		/** End of message indicator */
		uint8_t end_of_msgi : 1;
		/** Connector change indicator */
		uint8_t cci : 7;
		/** Data length */
		uint8_t data_len;
		/** Vendor defined message indicator */
		uint8_t vdmi : 1;
		/** Reserved */
		uint8_t Reserved : 6;
		/** Security request indicator */
		uint8_t sri : 1;
		/** Command status */
		uint8_t cmd_sts;
	};
	uint8_t raw_value[PDC_MAX_CCI_LENGTH];
};

#endif /* __CROS_EC_PDC_RTS54XX_H */
