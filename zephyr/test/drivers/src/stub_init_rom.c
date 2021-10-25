/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

__attribute__((weak)) const void *init_rom_map(const void *addr, int size)
{
	return addr;
}

__attribute__((weak)) void init_rom_unmap(const void *addr, int size)
{
}

__attribute__((weak)) int init_rom_copy(int offset, int size, char *data)
{
	return 0;
}
