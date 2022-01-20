/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common X86 chipset power control module for Chrome EC */

#ifndef __CROS_EC_COMMON_X86_H
#define __CROS_EC_COMMON_X86_H

#include "chipset.h"
#include "chipset_config.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "lpc.h"
#include "power.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "vboot.h"
#include "wireless.h"

#ifdef CONFIG_POWER_S0IX
/*
 * Restore host event masks for SMI and SCI when host exits S0ix. This is done
 * because BIOS is not involved in the resume path and so EC needs to restore
 * the masks from backup variables.
 */
void lpc_s0ix_resume_restore_masks(void);

/*
 * Wake up the AP if hang detected entering S0ix
 */
void lpc_s0ix_hang_detected(void);
#endif

/**
 * Introduces SYS_RESET_L Debounce time delay
 *
 * The default implementation is to wait for a duration of 32 ms.
 * If a board needs a different debounce time delay, they may override
 * this function
 */
__override_proto void x86_sys_reset_delay(void);

#endif /* __CROS_EC_COMMON_X86_H */
