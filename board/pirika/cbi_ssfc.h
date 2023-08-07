/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _DEDEDE_CBI_SSFC__H_
#define _DEDEDE_CBI_SSFC__H_

#include "stdint.h"
#include "stdbool.h"

/****************************************************************************
 * Dedede CBI Second Source Factory Cache
 */

/*
 * DB type (Bits 0-3)
 */
enum ec_ssfc_db_type {
	SSFC_DB_NONE = 0,
	SSFC_DB_2C = 1,
	SSFC_DB_1C_LTE = 2,
	SSFC_DB_1A_HDMI = 3,
	SSFC_DB_1C_1A = 4,
	SSFC_DB_LTE_HDMI = 5,
	SSFC_DB_1C_1A_LTE = 6,
	SSFC_DB_1C = 7
};

/*
 *  Support BC1.2 (Bits 19)
 */
enum ec_ssfc_bc12_support { SSFC_BC12_SUPPORT = 0, SSFC_BC12_NONE = 1 };

union pirika_cbi_ssfc {
	struct {
		uint32_t db_type : 4;
		uint32_t reserved_1 : 15;
		uint32_t bc12 : 1;
		uint32_t reserved_2 : 12;
	};
	uint32_t raw_value;
};

/**
 * Get the DB type from SSFC_CONFIG.
 *
 * @return the DB type.
 */
enum ec_ssfc_db_type get_cbi_ssfc_db_type(void);

/**
 * Get BC12 type from SSFC_CONFIG.
 *
 * @return the BC12 type.
 */
bool get_cbi_ssfc_bc_support(void);

#endif /* _DEDEDE_CBI_SSFC__H_ */
