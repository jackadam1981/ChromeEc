/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Cherry SCP configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#include "baseboard.h"

/*
 * RW only, no flash
 * +-------------------- 0xaf000 + 0
 * | ROM vectortable, .text, .rodata, .data LMA
 * +-------------------- 0xaf000 + 0xb000 = 0xba000
 * | RAM .bss, .data
 * +-------------------- 0xaf000 + 0xfc00 = 0xbec00
 * | Reserved (padding for 1k-alignment)
 * +-------------------- 0xaf000 + 0xfdb0 = 0xbedb0
 * | IPI shared buffer with AP (288 + 8) * 2
 * +-------------------- 0xaf000 + 0x10000 = 0xbf000
 *
 * [Memory remap]
 * The base address 0x0~0x1000 is translated to 0xaf000~0xbf000. This means
 * that core 1 actually accesses physical address 0xaf000 when accesses 0x0
 * from core view.
 */
#define CONFIG_ROM_BASE 0x0
#define CONFIG_RAM_BASE 0xb000
#define CONFIG_ROM_SIZE (CONFIG_RAM_BASE - CONFIG_ROM_BASE)
#define CONFIG_RAM_SIZE ((CONFIG_IPC_SHARED_OBJ_ADDR & (~(0x400 - 1))) - \
			 CONFIG_RAM_BASE)

/* SCP_FW_END is used to calc the base of IPI buffer for AP.
 * Provide AP view physical address which include the offset.
 */
#define SCP_FW_END 0xbf000

#endif /* __CROS_EC_BOARD_H */
