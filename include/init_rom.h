/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Routines for accessing data objects store in the .init_rom region.
 * Enabled with the CONFIG_CHIP_INIT_ROM_REGION config option. Data
 * ojects are placed into the .init_rom region using the __init_rom attribute.
 */

#ifndef __CROS_EC_INIT_ROM_H
#define __CROS_EC_INIT_ROM_H

#include "common.h"

/**
 * Get the memory mapped address of an .init_rom data object;
 *
 * @param offset	Flash offset of the data object.
 * @param size	        Size of the data object.
 *
 * @return Pointer to data object in memory. Return NULL if the object
 * is not memory mapped.
 */
const uint8_t *init_rom_get_addr(int offset, int size);

/**
 * Lock or unlock the init rom area. Required before using init_rom_copy()
 *
 * @param lock	If 0, unlock the init_rom area, otherwise lock
 */
void init_rom_lock(int lock);

/**
 * Lock or unlock the init rom area. Required before using init_rom_copy()
 *
 * @param offset	Flash offset of the data object.
 * @param size	        Size of the data object.
 * @param data		Destination buffer for data.
 *
 * @return 0 on success.
 */
int init_rom_copy(int offset, int size, char *data);

#endif /* __CROS_EC_INIT_ROM_H */
