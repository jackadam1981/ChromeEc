/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_H__
#define __X86_NON_DSX_COMMON_H__

#include <zephyr/types.h>
#include <x86_common_pwrseq.h>

#define DT_DRV_COMPAT intel_ap_pwrseq
#define INTEL_COM_POWER_NODE	DT_INST(0, intel_ap_pwrseq)
/* Check if the GPIO is present */
#define POWER_SEQ_GPIO_PRESENT(pha) \
	DT_PHA_HAS_CELL(INTEL_COM_POWER_NODE, pha, flags)

/* dt_flags is 8-bits, so need a cast without which the compile will fail */
#define POWER_GPIO_DT_SPEC_GET(prop)              \
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
#if POWER_SEQ_GPIO_PRESENT(en_pp5000_s5_gpios)
	const struct gpio_dt_spec enable_pp5000_a;
#endif
#if POWER_SEQ_GPIO_PRESENT(en_pp3300_s5_gpios)
	const struct gpio_dt_spec enable_pp3300_a;
#endif
	const struct gpio_dt_spec pg_ec_rsmrst_odl;
	const struct gpio_dt_spec ec_pch_rsmrst_odl;
#if POWER_SEQ_GPIO_PRESENT(pg_ec_dsw_pwroks_gpios)
	const struct gpio_dt_spec pg_ec_dsw_pwrok;
#endif
#if POWER_SEQ_GPIO_PRESENT(ec_soc_dsw_pwrok_gpios)
	const struct gpio_dt_spec ec_soc_dsw_pwrok;
#endif
	const struct gpio_dt_spec slp_s3_l;
	const struct gpio_dt_spec all_sys_pwrgd;
	const struct gpio_dt_spec slp_sus_l;
};

/*
 * @brief Create power sequencing thread
 *
 * @param p1 Thread sleep time in ms
 * TODO: Add details about inputs
 */
void pwrseq_thread(void *p1, void *p2, void *p3);

extern struct gpio_config power_seq_gpios[];
extern const int power_seq_gpios_count;
extern enum power_states_ndsx chipset_pwr_sm_run(
				enum power_states_ndsx curr_state);
extern void chipset_force_shutdown(enum chipset_shutdown_reason reason);
extern void chipset_reset(enum chipset_shutdown_reason reason);
extern void init_chipset_pwr_seq_state(void);
extern void all_sig_pass_thru_handler(void);
extern void common_rsmrst_pass_thru_handler(void);
extern void pwr_sm_set_state(enum power_states_ndsx new_state);
#endif /* __X86_NON_DSX_COMMON_H__ */
