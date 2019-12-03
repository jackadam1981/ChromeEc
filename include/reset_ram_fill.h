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
 * 	if (*reset_ram_fill_cond0_reg & reset_ram_fill_cond0_mask)
 * 		(Do RAM Fill)
 */

#ifndef __RESET_RAM_FILL_H
#define __RESET_RAM_FILL_H

#include <stdint.h>

/*
 * Configuring chip specific reset cause register and
 * reset cause power on reset mask.
 */
extern volatile uint32_t *const reset_ram_fill_cond0_reg;
extern const uint32_t reset_ram_fill_cond0_mask;

/*
 * Allow configuring chip/ec specific condition register and mask.
 * This is intended to be implemented with chip backup registers.
 */
extern volatile uint32_t *const reset_ram_fill_cond1_reg;
extern const uint32_t reset_ram_fill_cond1_mask;

#endif /* __RESET_RAM_FILL_H */