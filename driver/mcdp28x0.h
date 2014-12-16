/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Megachips DisplayPort to HDMI protocol converter / level shifter driver.
 */

#ifndef MCDP28X0_H
#define MCDP28X0_H

#include <util.h>

#define MCDP_OUTBUF_MAX 64
#define MCDP_INBUF_MAX 64

struct mcdp_version {
	uint8_t major;
	uint8_t minor;
	uint16_t build;
};

struct mcdp_info {
	uint16_t family;
	uint16_t chipid;
	struct mcdp_version irom;
	struct mcdp_version fw;
};

/**
 * initialize mcdp driver.
 */
void mcdp_init(void);

/**
 * get get information command from mcdp.
 *
 * @info pointer to mcdp_info structure
 * @return zero if success, error code otherwise.
 */
int mcdp_get_info(struct mcdp_info  *info);

#endif
