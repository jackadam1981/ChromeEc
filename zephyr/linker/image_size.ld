/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if defined(CONFIG_CPU_CORTEX_M)
__image_size = ADDR(rodata) + SIZEOF(rodata) - ADDR(rom_start) + SIZEOF(datas) +
	       SIZEOF(devices) + SIZEOF(k_mutex_area);
#else
__image_size = 0;
#endif
