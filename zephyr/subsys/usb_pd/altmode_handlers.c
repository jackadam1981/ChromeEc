/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/syscall_handler.h>
#include "intel_altmode.h"

static inline int z_vrfy_pd_altmode_read(const struct device *dev, union data_status_reg *data)
{
	return z_impl_pd_altmode_read(dev, data);
}
