/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INTEL_ALTMODE_H
#define __EMUL_INTEL_ALTMODE_H

#include <zephyr/device.h>
#include <drivers/intel_altmode.h>

int emul_pd_altmode_set_status(const struct device *dev,
			       const union data_status_reg data);

#endif /*__EMUL_INTEL_ALTMODE_H */
