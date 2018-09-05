/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kukui SCP configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#define CONFIG_FLASH_SIZE 0x40000 /* Image file size: 256KB */
#undef  CONFIG_LID_SWITCH
#undef  CONFIG_FW_INCLUDE_RO

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
