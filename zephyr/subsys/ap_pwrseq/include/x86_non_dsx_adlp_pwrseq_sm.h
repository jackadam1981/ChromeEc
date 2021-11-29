/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Chrome OS-specific Alderlake (ADL) Soc power sequencing
 */

#ifndef __X86_NON_DSX_ADLP_H__
#define __X86_NON_DSX_ADLP_H__

#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include <logging/log.h>

/* TODO: These delays can be configured from .dts */
#define SYS_PWROK_DELAY_MS 45
#define PCH_PWROK_DELAY_MS 2
#define VRRDY_TIMEOUT_MS 50
#define VCCST_PWRGD_DELAY_MS 2

extern void gpio_set_lvl(const char *net_name, int val);
extern int gpio_get_lvl(const char *net_name);

/* Power signals list */
enum power_signal {
	X86_SLP_S0_DEASSERTED,
#if 0
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
#endif
	X86_SLP_SUS_DEASSERTED,
	X86_RSMRST_L_PGOOD,
	X86_DSW_PWROK,
	X86_ALL_SYS_PGOOD,
	/* Number of X86 signals */
	POWER_SIGNAL_COUNT
};

/* Power sequencing GPIOs */
/* TODO:- Add more config to add alderlake specific variants
 * without power sequencer chips
 */
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
};

const int power_seq_gpios_count = ARRAY_SIZE(power_seq_gpios);

#define ADL_INT_GPIO_DEV(node) \
	DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(node), gpios, 0))
struct gpio_interrupt_config power_seq_int_gpios[] = {
	{
		//.net_name = GPIO_NET_NAME(PCH_EC_SLP_SUS_L),
		.gpio_dev = ADL_INT_GPIO_DEV(PCH_EC_SLP_SUS_L),
		.int_flags = GPIO_INT_EDGE_BOTH,
		.pin = DT_GPIO_PIN(DT_NODELABEL(PCH_EC_SLP_SUS_L), gpios),
	},

	{
		//.net_name = GPIO_NET_NAME(PCH_EC_SLP_S0_L),
		.gpio_dev = ADL_INT_GPIO_DEV(PCH_EC_SLP_S0_L),
		/* POWER_SIGNAL_DISABLE_AT_BOOT */
		.int_flags = GPIO_INT_EDGE_BOTH,
		.pin = DT_GPIO_PIN(DT_NODELABEL(PCH_EC_SLP_S0_L), gpios),
	},

	{
		//.net_name = GPIO_NET_NAME(VR_PG_EC_RSMRST_ODL),
		.gpio_dev = ADL_INT_GPIO_DEV(VR_PG_EC_RSMRST_ODL),
		.int_flags = GPIO_INT_EDGE_BOTH,
		.pin = DT_GPIO_PIN(DT_NODELABEL(VR_PG_EC_RSMRST_ODL), gpios),
	},

	{
		//.net_name = GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD),
		.gpio_dev = ADL_INT_GPIO_DEV(VR_EC_ALL_SYS_PWRGD),
		.int_flags = GPIO_INT_EDGE_BOTH,
		.pin = DT_GPIO_PIN(DT_NODELABEL(VR_EC_ALL_SYS_PWRGD), gpios),
	},

#if POWER_SEQ_GPIO_PRESENT(VR_EC_DSW_PWROK)
	{
		//.net_name = GPIO_NET_NAME(VR_EC_DSW_PWROK),
		.gpio_dev = ADL_INT_GPIO_DEV(VR_EC_DSW_PWROK),
		.int_flags = GPIO_INT_EDGE_BOTH,
		.pin = DT_GPIO_PIN(DT_NODELABEL(VR_EC_DSW_PWROK), gpios),
	},
#endif
};

const int power_seq_int_gpios_count = ARRAY_SIZE(power_seq_int_gpios);

#endif /* __X86_NON_DSX_ADLP_H__ */
