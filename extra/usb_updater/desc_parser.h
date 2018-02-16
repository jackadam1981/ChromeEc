/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EXTRA_USB_UPDATER_DESC_PARSER_H
#define __EXTRA_USB_UPDATER_DESC_PARSER_H

#include <stddef.h>
#include <stdint.h>

struct result_node {
	size_t size;
	uint8_t *expected_result; /* Points to a 'size' bytes. */
	uint8_t *mask_off;  /* Bytes/bits to ignore in the result. */
};

enum range_type_t {
	AP_RANGE,
	EC_RANGE,
	EC_GANG_RANGE
};

struct addr_range {
	uint32_t base_addr;
	uint32_t range_size;
	struct result_node *variants;
};

struct all_ranges {
	enum range_type_t range_type;
	size_t range_count;
	size_t variant_count;
	struct addr_range *ranges;
};

struct fwv_descriptor {
	char board_id[4];
	struct all_ranges ranges[2]; /* One for AP and one for EC. */
};

struct boards {
	size_t board_count;
	struct fwv_descriptor *fwd;
};

struct boards *get_board_descriptions(const char *desc_file);

#endif // __EXTRA_USB_UPDATER_DESC_PARSER_H
