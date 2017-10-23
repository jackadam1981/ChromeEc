/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

enum vpd_type {
	VPD_TYPE_TERMINATOR = 0,
	VPD_TYPE_STRING,
	VPD_TYPE_INFO = 0xfe,
	VPD_TYPE_IMPLICIT_TERMINATOR = 0xff,
};

struct vpd {
	uint8_t data[CONFIG_RO_VPD_SIZE];
};

extern const uint8_t *vpd_copy;

/**
 * Initialize VPD API
 */
void vpd_init(void);

/**
 * Get the value of the key from VPD
 *
 * @param type      Type of <key>
 * @param key       Key to search
 * @param value     (OUT) Value to be returned
 * @param value_len (OUT) Length of <value>
 * @return EC_ERROR_* on error or EC_SUCCESS otherwise.
 */
int vpd_get(enum vpd_type type, const uint8_t *key,
	    uint8_t *value, uint8_t *value_len);
