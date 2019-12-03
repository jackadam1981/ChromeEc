/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_RAM_TABLE_H
#define __CROS_EC_RAM_TABLE_H

#include <stddef.h>

struct mem_range {
	void *start;
	void *end;
};

extern const struct mem_range ram_table[];
extern const size_t ram_table_size;

#endif /* __CROS_EC_RAM_TABLE_H */
