/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_ADLP_H__
#define __X86_NON_DSX_ADLP_H__

#include <logging/log.h>
#include <x86_common_pwrseq.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

extern struct power_seq_context pwrseq_ctx;

extern void gpio_set_lvl(const char *net_name, int val);
extern int gpio_get_lvl(const char *net_name);
extern struct common_pwrseq_config com_cfg;

struct chipset_pwrseq_config {
	int pch_pwrok_delay_ms;
	int sys_pwrok_delay_ms;
	int sys_reset_delay_ms;
	int vccst_pwrgd_delay_ms;
	int vrrdy_timeout_ms;
};

/* Power sequencing GPIOs */
struct gpio_config power_seq_gpios[] = {
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_SUS_L),
	},
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_S0_L),
	},
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_S3_L),
	},
	{
		POWER_SEQ_GPIO(VR_PG_EC_RSMRST_ODL),
	},
	{
		POWER_SEQ_GPIO(VR_EC_ALL_SYS_PWRGD),
	},
#if POWER_SEQ_GPIO_PRESENT(VR_EC_DSW_PWROK)
	{
		POWER_SEQ_GPIO(VR_EC_DSW_PWROK),
	},
#endif
	{
		POWER_SEQ_GPIO(EC_PCH_RSMRST_L),
	},
#if POWER_SEQ_GPIO_PRESENT(EC_PCH_DSW_PWROK)
	{
		POWER_SEQ_GPIO(EC_PCH_DSW_PWROK),
	},
#endif
#if POWER_SEQ_GPIO_PRESENT(EC_PCH_SYS_PWROK)
	{
		POWER_SEQ_GPIO(EC_PCH_SYS_PWROK),
	},
#endif
	{
		POWER_SEQ_GPIO(EC_PCH_PWR_BTN_ODL),
	},
#if POWER_SEQ_GPIO_PRESENT(EC_VR_EN_PP5000_A)
	{
		POWER_SEQ_GPIO(EC_VR_EN_PP5000_A),
	},
#endif
#if POWER_SEQ_GPIO_PRESENT(IMVP9_VRRDY_OD)
	{
		POWER_SEQ_GPIO(IMVP9_VRRDY_OD),
	},
#endif
#if POWER_SEQ_GPIO_PRESENT(VCCST_PWRGD_OD)
	{
		POWER_SEQ_GPIO(VCCST_PWRGD_OD),
	},
#endif
	{
		POWER_SEQ_GPIO(PCH_PWROK),
	},
	{
		POWER_SEQ_GPIO(SYS_RESET_L),
	},
};

const int power_seq_gpios_count = ARRAY_SIZE(power_seq_gpios);
#endif /* __X86_NON_DSX_ADLP_H__ */
