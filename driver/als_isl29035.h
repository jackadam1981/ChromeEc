/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Intersil ILS29035 light sensor driver
 */

#ifndef __CROS_EC_ALS_ISL29035_H
#define __CROS_EC_ALS_ISL29035_H

int isl29035_read_lux(int *lux, int af);

#ifdef CONFIG_ALS_INTERRUPTS
int isl29035_set_interrupt(unsigned int lower_threshold,
		unsigned int upper_threshold, int af);

int isl29035_interrupt_handler(void);
#endif

#endif	/* __CROS_EC_ALS_ILS29035_H */
