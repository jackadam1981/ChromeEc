/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CLOCK_CHIP_H
#define __CROS_EC_CLOCK_CHIP_H

/**
 * Set the CPU clock to maximum freq for better performance.
 */
void clock_turbo(void);

/**
 * Set the CPU clock back to normal freq.
 */
void clock_turbo_disable(void);

#endif /* __CROS_EC_CLOCK_CHIP_H */
