/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Interface for setting up chip/ec specific ram fill triggers
 *
 * Since each chip has different registers for representing power-on-reset
 * detection and handling backup registers, let each chip specify their
 * specific registers+masks that will trigger a ram fill in the reset vector.
 *
 * The reset vector will use the following loose formula for each condition
 * to determine if a fill should occur:
 *
 * if (*reset_ram_fill_cond0_reg != 0xFFFFFFFF)
 * 	int test = *reset_ram_fill_cond0_reg & reset_ram_fill_cond0_mask;
 * 	if (test == *reset_ram_fill_cond0_test)
 * 		(Do RAM Fill)
 */

#ifndef __RESET_RAM_FILL_H
#define __RESET_RAM_FILL_H

#include <stdint.h>

/**
 * Allow configuring multiple triggers for the memory fill.
 *
 * The intended use of these condition is as follows:
 * - Condition 0 - Detect Power-On-Reset
 * - Condition 1 - Detect Pin-Reset
 * - Condition 2 - Software triggering
 */

extern volatile uint32_t *const reset_ram_fill_cond0_reg;
extern const uint32_t reset_ram_fill_cond0_mask;
extern const uint32_t reset_ram_fill_cond0_test;

extern volatile uint32_t *const reset_ram_fill_cond1_reg;
extern const uint32_t reset_ram_fill_cond1_mask;
extern const uint32_t reset_ram_fill_cond1_test;

extern volatile uint32_t *const reset_ram_fill_cond2_reg;
extern const uint32_t reset_ram_fill_cond2_mask;
extern const uint32_t reset_ram_fill_cond2_test;

#endif /* __RESET_RAM_FILL_H */