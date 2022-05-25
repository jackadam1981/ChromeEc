/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Cherry SCP configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "baseboard.h"

#define SCP_CORE1_RAM_SIZE 0x10000 /* 64K */
#define SCP_CORE1_RAM_PADDING 0xc00 /* for 4K-alignment */

/*
 * RW only, no flash
 * +-------------------- 0x0
 * | ROM vectortable, .text, .rodata, .data LMA
 * +-------------------- 0x68000
 * | RAM .bss, .data
 * +-------------------- 0xaf000 (4k-alignment)
 * | SCP core1 boot code
 * +-------------------- 0xbf000 (4k-alignment)
 * | Reserved (padding for 4k-alignment)
 * +-------------------- 0xbfc00
 * | Reserved (padding for 1k-alignment)
 * +-------------------- 0xbfdb0
 * | IPI shared buffer with AP (288 + 8) * 2
 * +-------------------- 0xc0000
 */
#define CONFIG_ROM_BASE 0x0
#define CONFIG_RAM_BASE 0x68000
#define CONFIG_ROM_SIZE (CONFIG_RAM_BASE - CONFIG_ROM_BASE)
#define CONFIG_RAM_SIZE ((CONFIG_IPC_SHARED_OBJ_ADDR & (~(0x400 - 1))) - \
			 CONFIG_RAM_BASE - \
			 SCP_CORE1_RAM_SIZE - \
			 SCP_CORE1_RAM_PADDING)

#define SCP_FW_END 0xc0000

#endif /* __CROS_EC_BOARD_H */
