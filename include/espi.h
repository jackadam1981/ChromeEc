/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* eSPI module for Chrome EC */

#ifndef __CROS_EC_ESPI_H
#define __CROS_EC_ESPI_H

#include "gpio_signal.h"

/* Signal through VW */
enum espi_vw_signal {
	VW_SIGNAL_BASE = GPIO_COUNT,
	VW_SLP_S3_L,			/* index 02h (In)  */
	VW_SLP_S4_L,
	VW_SLP_S5_L,
	VW_SUS_STAT_L,			/* index 03h (In)  */
	VW_PLTRST_L,
	VW_OOB_RST_WARN,
	VW_OOB_RST_ACK,			/* index 04h (Out) */
	VW_WAKE_L,
	VW_PME_L,
	VW_ERROR_FATAL,			/* index 05h (Out) */
	VW_ERROR_NON_FATAL,
	/* Merge bit 3/0 into one signal. Need to set them simultaneously */
	VW_SLAVE_BTLD_STATUS_DONE,
	VW_SCI_L,			/* index 06h (Out) */
	VW_SMI_L,
	VW_RCIN_L,
	VW_HOST_RST_ACK,
	VW_HOST_RST_WARN,		/* index 07h (In)  */
	VW_SUS_ACK,			/* index 40h (Out) */
	VW_SUS_WARN_L,			/* index 41h (In)  */
	VW_SUS_PWRDN_ACK_L,
	VW_SLP_A_L,
	VW_SLP_LAN,                     /* index 42h (In)  */
	VW_SLP_WLAN,
	VW_SIGNAL_BASE_END,
};

#define VW_SIGNAL_COUNT (VW_SIGNAL_BASE_END - VW_SIGNAL_BASE - 1)

/**
 * Set eSPI Virtual-Wire signal to Host
 *
 * @param signal vw signal needs to set
 * @param level  level of vw signal
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_set_wire(enum espi_vw_signal signal, uint8_t level);

/**
 * Get eSPI Virtual-Wire signal from host
 *
 * @param signal vw signal needs to get
 * @return      1: set by host, otherwise: no signal
 */
int espi_vw_get_wire(enum espi_vw_signal signal);

/**
 * Enable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to enable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_enable_wire_int(enum espi_vw_signal signal);

/**
 * Disable VW interrupt of power sequence signal
 *
 * @param signal vw signal needs to disable interrupt
 * @return EC_SUCCESS, or non-zero if error.
 */
int espi_vw_disable_wire_int(enum espi_vw_signal signal);

/**
 * Return pointer to constant eSPI virtual wire signal name
 *
 * @param signal virtual wire enum
 * @return pointer to string or NULL if signal out of range
 */
const char *espi_vw_get_wire_name(enum espi_vw_signal signal);

/**
 * Check if signal is an eSPI virtual wire
 * @param signal is gpio_signal or espi_vw_signal enum casted to int
 * @return 1 if signal is virtual wire else returns 0.
 */
int espi_signal_is_vw(int signal);


#ifdef CONFIG_HOSTCMD_ESPI_OOB
/* OOB channel SMBus command */
enum espi_oob_smbus_command {
	OOB_SMBUS_GET_TEMP = 1,
	OOB_SMBUS_GET_RTC,
};

/* OOB channel receive length */
#define OOB_SMBUS_REC_TEMP_LENGTH	0x05
#define OOB_SMBUS_REC_RTC_LENTGH	0x0C

/* OOB channel SMBus receive data byte count */
#define OOB_SMBUS_REC_TEMP_BYTE_COUNT	0x02
#define OOB_SMBUS_REC_RTC_BYTE_COUNT	0x09

/* eSPI cycle type field */
#define ESPI_FLASH_READ_CYCLE_TYPE	0x00
#define ESPI_FLASH_WRITE_CYCLE_TYPE	0x01
#define ESPI_FLASH_ERASE_CYCLE_TYPE	0x02
#define ESPI_OOB_CYCLE_TYPE		0x21

/* eSPI tag + len[11:8] field */
#define ESPI_TAG_LEN_FIELD(tag, len) \
		   ((((tag) & 0xF) << 4) | (((len) >> 8) & 0xF))

/* OOB channel SMBus address field */

/* PCH/SoC eSPI-MC (hardware) Request Handler Address */
#define OOB_SMBUS_DEST_ADDR_PCH_SOC_MC		(0x01 << 1)

/* PCH/SoC Intel ME FW SMBus Request Handler Address */
#define OOB_SMBUS_DEST_ADDR_PCH_SOC_ME		(0x10 << 1)

/* Intel PCH PECI SMBus MCTP Request Handler Address */
#define OOB_SMBUS_DEST_ADDR_PCH_PECI_MCTP	(0x20 << 1)

/* eSPI slave address to master */
#define OOB_SMBUS_SRC_ADDR_EC	0x07

/**
 * eSPI OOB channel data buffer size maximum 80 bytes.
 * Actually length field can reach 4096 bytes defined in intel espi spec.
 */
#define ESPI_OOB_MAX_LENGTH	80

/* Minimum eSPI OOB channel data receive length */
#define ESPI_OOB_MIN_LENGTH	5

/**
 * Receive messages from eSPI OOB channel
 * @param oob_data point to the oob data array buffer
 * @return EC_SUCCESS, or non-zero if error
 */
int espi_oob_receive(uint8_t *oob_data);

/**
 * Send messages via eSPI OOB channel
 * @param oob_data point to the oob data array buffer
 * @return EC_SUCCESS, or non-zero if error
 */
int espi_oob_send(uint8_t *oob_data);
#endif

#endif  /* __CROS_EC_ESPI_H */
