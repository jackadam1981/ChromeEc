/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Embed firmware version number in the binary */

#include <stdint.h>
#include "common.h"
#include "compile_time_macros.h"
#include "ec_version.h"
#include "stddef.h"
#include "util.h"
#include "system.h"
#include "version.h"

#include "console.h"

BUILD_ASSERT(CONFIG_ROLLBACK_VERSION >= 0);
BUILD_ASSERT(CONFIG_ROLLBACK_VERSION <= INT32_MAX);

/*
 * Version string format is
 *   <board>_v<major>.<minor>.<commits>[dirty]g<hash>
 * where
 *   <board>: board name (e.g. nami)
 *   <major>: major version number
 *   <minor>: minor version number
 *   <commits>: number of commits
 *   <hash>: Git hash of HEAD
 *   <dirty>: '+' if image contains uncommitted changes or '-' otherwise
 */
const struct image_data __keep current_image_data
	__attribute__((section(".rodata.ver"))) = {
	.cookie1 = CROS_EC_IMAGE_DATA_COOKIE1,
	.version = CROS_EC_VERSION32,
#ifndef TEST_BUILD
	.size = (const uintptr_t)&__image_size,
#endif
	.rollback_version = CONFIG_ROLLBACK_VERSION,
	.cookie2 = CROS_EC_IMAGE_DATA_COOKIE2,
};

const char build_info[] __keep __attribute__((section(".rodata.buildinfo"))) =
	VERSION " " DATE " " BUILDER;

static int get_num_commits(const struct image_data *data)
{
	int numperiods = 0;
	int ret = 0;
	size_t i;

	/* Version string format is name_major.branch.commits-hash[dirty] */
	for (i = 0; i < sizeof(data->version); i++) {
		if (data->version[i] == '.') {
			numperiods++;
			if (numperiods == 2)
				break;
		}
	}

	i++;
	for (; i < sizeof(data->version); i++) {
		int d;
		if (data->version[i] == '-')
			break;
		ret *= 10;
		d = data->version[i] - '0';
		if (d < 0 || d > 9)
			return 0;
		ret += d;
	}

	return (i == sizeof(data->version) ? 0 : ret);

}

static uint16_t get_version(const struct image_data *data)
{
	char *p, *q, *e;
	/* Assume board name does not contain "_v" */
	const char *mark = "_v";
	int major, minor;
	char buf[sizeof(data->version)];

	memcpy(buf, data->version, sizeof(data->version));

	/* Search for "..._v" */
	p = strstr(buf, mark);
	if (!p)
		return 0;

	/* Set pointer the beginning of <major> and search for '.' */
	p += strlen(mark);
	q = strstr(p, ".");
	if (!q)
		return 0;

	/* Convert string <major> to integer */
	*q = '\0';
	major = strtoi(p, &e, 10);
	if (*e || major > UINT8_MAX)
		return 0;

	/* Set pointer to the beginning of <minor> and search for 2nd '.' */
	p = q + 1;
	q = strstr(p, ".");

	/* Convert string <minor> to integer */
	*q = '\0';
	minor = strtoi(p, &e, 10);
	if (*e || minor > UINT8_MAX)
		return 0;

	return (major << 8) | minor;
}

static const struct image_data *get_image_data(enum system_image_copy_t copy)
{
	if (IS_ENABLED(CONFIG_COMMON_RUNTIME))
		return system_get_image_data(copy);
	else
		return &current_image_data;
}

int ver_get_num_commits(enum system_image_copy_t copy)
{
	const struct image_data *data = get_image_data(copy);
	return data ? get_num_commits(data) : 0;
}

uint16_t ver_get_version(enum system_image_copy_t copy)
{
	const struct image_data *data = get_image_data(copy);
	return data ? get_version(data) : 0;
}
