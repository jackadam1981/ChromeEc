/*
 * Copyright (c) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __X86_NON_DSX_ADLP_H__
#define __X86_NON_DSX_ADLP_H__

#include <logging/log.h>
#include <x86_common_pwrseq.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

/* TODO: These delays can be configured from .dts */
#define SYS_PWROK_DELAY_MS 45
#define PCH_PWROK_DELAY_MS 2
#define VRRDY_TIMEOUT_MS 50
#define VCCST_PWRGD_DELAY_MS 2

extern void gpio_set_lvl(const char *net_name, int val);
extern int gpio_get_lvl(const char *net_name);

/* Power sequencing GPIOs */
/* TODO:- Add more config to add alderlake specific variants
 * without power sequencer chips */
struct gpio_config power_seq_gpios[] = {
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_SUS_L),
	},
	{
		POWER_SEQ_GPIO(PCH_EC_SLP_S0_L),
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

struct gpio_interrupt_config power_seq_intr_gpios[] = {
	{
		POWER_SEQ_INTR_GPIO(PCH_EC_SLP_S0_L),
		/* Disable interrupt at boot up */
		.disable_at_boot = true,
	},
	{
		POWER_SEQ_INTR_GPIO(PCH_EC_SLP_SUS_L),
		.disable_at_boot = false,
	},
	{
		POWER_SEQ_INTR_GPIO(VR_PG_EC_RSMRST_ODL),
		.disable_at_boot = false,
	},
#if POWER_SEQ_GPIO_PRESENT(VR_EC_DSW_PWROK)
	{
		POWER_SEQ_INTR_GPIO(VR_EC_DSW_PWROK),
		.disable_at_boot = false,
	},
#endif
	{
		POWER_SEQ_INTR_GPIO(VR_EC_ALL_SYS_PWRGD),
		.disable_at_boot = false,
	},
};
const int power_seq_intr_gpios_count = ARRAY_SIZE(power_seq_intr_gpios);

#endif /* __X86_NON_DSX_ADLP_H__ */
