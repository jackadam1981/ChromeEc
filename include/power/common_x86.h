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

/* GPIO for power signal */
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_S3
#define SLP_S3_SIGNAL_L VW_SLP_S3_L
#else
#define SLP_S3_SIGNAL_L GPIO_PCH_SLP_S3_L
#endif
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_S4
#define SLP_S4_SIGNAL_L VW_SLP_S4_L
#else
#define SLP_S4_SIGNAL_L GPIO_PCH_SLP_S4_L
#endif
/*
 * The SLP_S5 signal has not traditionally been connected to the EC. If virtual
 * wire support is enabled, then SLP_S5 will be available that way. Otherwise,
 * use SLP_S4's GPIO as a proxy for SLP_S5. This matches old behavior and
 * effectively prevents S4 residency.
 */
#ifdef CONFIG_HOSTCMD_ESPI_VW_SLP_S5
#define SLP_S5_SIGNAL_L VW_SLP_S5_L
#else
#define SLP_S5_SIGNAL_L SLP_S4_SIGNAL_L
#endif

enum sys_sleep_state {
	SYS_SLEEP_S3,
	SYS_SLEEP_S4,
	SYS_SLEEP_S5,
#ifdef CONFIG_POWER_S0IX
	SYS_SLEEP_S0IX,
#endif
};

extern int power_s5_up;       /* Chipset is sequencing up or down */

extern const int sleep_sig[];

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
 * Provides custom logic for passthrough signals
 *
 * The default implementation is to return valid.  If a board needs
 * to perform additional checks or overrides on passthrough signals
 * they may override this function
 */
__override_proto bool is_passthrough_valid(enum gpio_signal pin_in,
		enum gpio_signal pin_out, int *p_in_level);

void handle_pass_through(enum gpio_signal pin_in,
		enum gpio_signal pin_out);

void handle_pass_through_with_callbacks(enum gpio_signal pin_in,
		enum gpio_signal pin_out,
		void (*before_write_callback)(int),
		void (*after_write_callback)(int));
/**
 * Introduces SYS_RESET_L Debounce time delay
 *
 * The default implementation is to wait for a duration of 32 ms.
 * If a board needs a different debounce time delay, they may override
 * this function
 */
__override_proto void x86_sys_reset_delay(void);

/**
 * Force chipset to G3 state.
 *
 * @return power_state New chipset state.
 */
__override_proto enum power_state chipset_force_g3(void);

/* Get system sleep state through GPIOs or VWs */
static inline int chipset_get_sleep_signal(enum sys_sleep_state state)
{
	return power_signal_get_level(sleep_sig[state]);
}

#endif /* __CROS_EC_COMMON_X86_H */
