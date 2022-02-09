/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_PWRSEQ_SM_HANDLER_H__
#define __X86_NON_DSX_COMMON_PWRSEQ_SM_HANDLER_H__

#include <x86_common_pwrseq.h>
#include <x86_non_dsx_espi.h>
#include <zephyr/types.h>

#define DT_DRV_COMPAT intel_ap_pwrseq
#define INTEL_COM_POWER_NODE	DT_INST(0, intel_ap_pwrseq)
/* Check if the GPIO is present */
#define PWRSEQ_GPIO_PRESENT(pha) \
	DT_PHA_HAS_CELL(INTEL_COM_POWER_NODE, pha, flags)

/* dt_flags is 8-bits, so need a cast without which the compile will fail */
#define PWRSEQ_GPIO_DT_SPEC_GET(prop)              \
{                                                          \
	.port = DEVICE_DT_GET(DT_GPIO_CTLR(DT_DRV_INST(0), prop)),\
	.pin = DT_GPIO_PIN(DT_DRV_INST(0), prop),                 \
	.dt_flags = (uint8_t)DT_GPIO_FLAGS(DT_DRV_INST(0), prop), \
}

/* Common device tree configurable attributes */
struct common_pwrseq_config {
	int pch_dsw_pwrok_delay_ms;
	int pch_pm_pwrbtn_delay_ms;
	int pch_rsmrst_delay_ms;
	int wait_signal_timeout_ms;
	int s5_timeout_s;
#if PWRSEQ_GPIO_PRESENT(en_pp5000_s5_gpios)
	const struct gpio_dt_spec enable_pp5000_a;
#endif
#if PWRSEQ_GPIO_PRESENT(en_pp3300_s5_gpios)
	const struct gpio_dt_spec enable_pp3300_a;
#endif
	const struct gpio_dt_spec pg_ec_rsmrst_odl;
	const struct gpio_dt_spec ec_pch_rsmrst_odl;
#if PWRSEQ_GPIO_PRESENT(pg_ec_dsw_pwrok_gpios)
	const struct gpio_dt_spec pg_ec_dsw_pwrok;
#endif
#if PWRSEQ_GPIO_PRESENT(ec_soc_dsw_pwrok_gpios)
	const struct gpio_dt_spec ec_soc_dsw_pwrok;
#endif
	const struct gpio_dt_spec slp_s3_l;
	const struct gpio_dt_spec all_sys_pwrgd;
	const struct gpio_dt_spec slp_sus_l;
};

/* Power signal GPIO pin flags */
#define POWER_SIGNAL_ACTIVE_STATE BIT(0)
#define POWER_SIGNAL_DISABLE_INT_ON_BOOT BIT(1)
#define POWER_SIGNAL_ACTIVE_LOW   0
#define POWER_SIGNAL_ACTIVE_HIGH  BIT(0)
/* Convert enum power_signal to a mask for signal functions */
#define POWER_SIGNAL_MASK(signal) (1 << (signal))
/* Input state flags. */
#define IN_PCH_SLP_S0_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S0_DEASSERTED)
#define IN_PCH_SLP_S3_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S3_DEASSERTED)
#define IN_PCH_SLP_S4_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S4_DEASSERTED)
#define IN_PCH_SLP_S5_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S5_DEASSERTED)
#define IN_PCH_SLP_SUS_DEASSERTED POWER_SIGNAL_MASK(X86_SLP_SUS_DEASSERTED)
#define IN_ALL_PM_SLP_DEASSERTED (IN_PCH_SLP_S3 | \
				  IN_PCH_SLP_S4 | \
				  IN_PCH_SLP_SUS)
#define IN_PGOOD_ALL_CORE POWER_SIGNAL_MASK(X86_DSW_PWROK)
#define IN_ALL_S0 (IN_PGOOD_ALL_CORE | IN_ALL_PM_SLP_DEASSERTED)
#define CHIPSET_G3S5_POWERUP_SIGNAL IN_PCH_SLP_SUS_DEASSERTED

/* The wait time is ~150 msec, allow for safety margin. */
#define IN_PCH_SLP_SUS_WAIT_TIME_MS 250
/*
 * Each board must provide its signal list and a corresponding enum
 * power_signal.
 */
extern struct power_signal_gpio_info power_signal_gpio_list[];
extern struct power_signal_vw_info power_signal_vw_list[];
extern const int power_signal_gpio_count;
extern const int power_signal_vw_count;

void power_update_signals(void);
enum power_states_ndsx chipset_pwr_sm_run(
				enum power_states_ndsx curr_state,
				const struct common_pwrseq_config *com_cfg);
void all_sig_pass_thru_handler(void);
void chipset_force_shutdown(enum pwrseq_chipset_shutdown_reason reason,
				const struct common_pwrseq_config *com_cfg);
void chipset_reset(enum pwrseq_chipset_shutdown_reason reason);
void common_rsmrst_pass_thru_handler(void);
void init_chipset_pwr_seq_state(void);
enum power_states_ndsx pwr_sm_get_state(void);
uint32_t pwrseq_get_input_signals(void);
void pwrseq_set_debug_signals(uint32_t signals);
uint32_t pwrseq_get_debug_signals(void);
void apshutdown(void);

extern const char pwrsm_dbg[][25];

#endif /* __X86_NON_DSX_COMMON_PWRSEQ_SM_HANDLER_H__ */
