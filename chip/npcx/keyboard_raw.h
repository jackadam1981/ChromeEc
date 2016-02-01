/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CHIP_NPCX_KEYBOARD_RAW_H
#define __CROS_EC_CHIP_NPCX_KEYBOARD_RAW_H

#include "include/keyboard_raw.h"

/**
 * Hibernate the raw keyboard interface.
 *
 * Calling this function puts the NPCX keyboard scan pins into a low power
 * hibernate compatible state.
 */
void keyboard_hibernate(void);

#endif  /* __CROS_EC_CHIP_NPCX_KEYBOARD_RAW_H */
