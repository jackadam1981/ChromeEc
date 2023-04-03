/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_DFU_H__
#define __CROS_EC_STARFISH_DFU_H__

#include <zephyr/toolchain.h>

/*
 * Sets up the device to enter DFU mode. The system will reboot.
 */
FUNC_NORETURN void dfu_enter(void);

#endif /* __CROS_EC_STARFISH_DFU_H__ */
