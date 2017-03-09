/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Version number for Chrome EC */

#ifndef __CROS_EC_VERSION_H
#define __CROS_EC_VERSION_H

#include "common.h"

#define CROS_EC_VERSION_COOKIE1 0xce112233
#define CROS_EC_VERSION_COOKIE2 0xce445566
#define CROS_EC_VERSION_COOKIE3 0xce778899

struct version_struct {
	uint32_t cookie1;
	char version[32];
	uint32_t cookie2;
#ifdef CONFIG_ROLLBACK
	uint32_t rollback_version;
	uint32_t cookie3;
#endif
} __packed;

extern const struct version_struct version_data;
extern const char build_info[];
extern const char __version_struct_offset[];

/**
 * Get the number of commits field from version string.
 */
uint32_t ver_get_numcommits(void);
#endif  /* __CROS_EC_VERSION_H */
