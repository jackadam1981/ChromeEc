/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SAR_H
#define __CROS_EC_SAR_H

#include "sar_sense.h"

struct sar_drv {
	int (*init)(const struct sar_sensor_t *s);

	int (*read)(const struct sar_sensor_t *s, int *status, int *irq_src);

	int (*set_resolution)(const struct sar_sensor_t *s, int res, int rnd);

	int (*get_resolution)(const struct sar_sensor_t *s, int *res);

	int (*set_sensitivity)(const struct sar_sensor_t *s, int type,
		int eng_val, int rnd);

	int (*get_sensitivity)(struct sar_sensor_t *s, int type, int *sens);

	int (*set_scanfreq)(const struct sar_sensor_t *s, int freq, int rnd);

	int (*get_scanfreq)(const struct sar_sensor_t *s, int *freq);

	int (*set_interrupt)(const struct sar_sensor_t *s,
		unsigned int threshold);
};

#endif /* __CROS_EC_SAR_H */
