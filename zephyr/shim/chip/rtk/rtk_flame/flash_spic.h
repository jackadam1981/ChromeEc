/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * For Chipset: RTK EC
 *
 * Function: RTK Flash Utility
 */

#ifndef __FLASH_SPIC_H__
#define __FLASH_SPIC_H__

/*********************
 *      INCLUDES
 *********************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**********************
 *      TYPEDEFS
 **********************/
typedef enum spic_status {
	SPIC_STATUS_ERROR = -1, /**< Generic error >*/
	SPIC_STATUS_INVALID_PARAMETER = -2, /**< The parameter is invalid >*/
	SPIC_STATUS_OK = 0, /**< Function executed sucessfully  >*/
} SPIC_STATUS_t;

typedef enum spic_bus_width {
	SPIC_CFG_BUS_SINGLE,
	SPIC_CFG_BUS_DUAL,
	SPIC_CFG_BUS_QUAD,
} SPIC_BUS_WIDTH_t;

typedef enum spic_address_size {
	SPIC_CFG_ADDR_SIZE_8,
	SPIC_CFG_ADDR_SIZE_16,
	SPIC_CFG_ADDR_SIZE_24,
	SPIC_CFG_ADDR_SIZE_32,
} SPIC_ADDRESS_SIZE_t;

typedef uint8_t SPIC_ALT_SIZE_t;

typedef struct qspi_command {
	struct {
		SPIC_BUS_WIDTH_t bus_width; /**< Bus width for the instruction
					       >*/
		uint8_t value; /**< Instruction value >*/
		uint8_t disabled; /**< Instruction phase skipped if disabled is
				     set to true >*/
	} instruction;
	struct {
		SPIC_BUS_WIDTH_t bus_width; /**< Bus width for the address >*/
		SPIC_ADDRESS_SIZE_t size; /**< Address size >*/
		uint32_t value; /**< Address value >*/
		uint8_t disabled; /**< Address phase skipped if disabled is set
				     to true >*/
	} address;
	struct {
		SPIC_BUS_WIDTH_t bus_width; /**< Bus width for alternative  >*/
		SPIC_ALT_SIZE_t size; /**< Alternative size >*/
		uint32_t value; /**< Alternative value >*/
		uint8_t disabled; /**< Alternative phase skipped if disabled is
				     set to true >*/
	} alt;
	uint8_t dumm_count; /**< Dumm cycles count >*/
	struct {
		SPIC_BUS_WIDTH_t bus_width; /**< Bus width for data >*/
	} data;
} SPIC_COMMAND_t;

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
SPIC_STATUS_t spic_init(uint8_t hz, uint8_t mode);
SPIC_STATUS_t spic_init_direct(uint8_t hz, uint8_t mode);
SPIC_STATUS_t spic_frequency(uint8_t hz);
SPIC_STATUS_t spic_write(const SPIC_COMMAND_t *command, const void *data,
			 uint32_t *length);
SPIC_STATUS_t spic_read(const SPIC_COMMAND_t *command, void *data,
			uint32_t *length);

#endif /* __FLASH_SPIC_H__ */
