/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_H__
#define __X86_NON_DSX_COMMON_H__

#include <zephyr/types.h>
#include <x86_common_pwrseq.h>

/* Power sequencing GPIOs */

#define PCH_EC_SLP_SUS_L	slpsus
#define PCH_EC_SLP_S0_L		slps0
#define PCH_EC_SLP_S3_L		slps3
#define PCH_EC_SLP_S4_L		slps4
#define PCH_EC_SLP_S5_L		slps5

/* DSW_PWROK is indication to PCH that 3P3V is stable */
#define VR_EC_DSW_PWROK		dswpwrokin
#define EC_PCH_DSW_PWROK	dswpwrokout

/* RSMRST is used for resetting primary power plane logic
 * When de-asserted, this signal is an indication that the
 * power wells are stable.
 */
#define VR_PG_EC_RSMRST_ODL	rsmrstin
#define EC_PCH_RSMRST_L		rsmrstout

/* Signal represents the power good for all the rest
 * of platform voltage rails.
 */
#define VR_EC_ALL_SYS_PWRGD	allsyspwrgd

#define PCH_PWROK		pchpwrok
/* SYS_PWROK is a generic power good input to the PCH is driven
 * and utilized in platform-specific manner
 */
#define EC_PCH_SYS_PWROK	syspwrok

#define EC_VR_PPVAR_VCCIN	vccin

/* EC to PCH indication system request to sleep / wake */
#define EC_PCH_PWR_BTN_ODL	pchpwrbtn

/* Enable 5V rails */
#define EC_VR_EN_PP5000_A	enpp5p0
/* Enable 3.3V rails */
#define EC_VR_EN_PP3300_A	enpp3p3

#define IMVP9_VRRDY_OD		imvp9vrrdy

/* TODO: Move to chipset */
#define VCCST_PWRGD_OD		vccstpwrgd

#define SYS_RESET_L		sysreset

/* GPIO net name as in schematics */
#define GPIO_NET_NAME(node)	DT_PROP(DT_NODELABEL(node), enum_name)

/* GPIO structure assignment */
#define POWER_SEQ_GPIO(node)	\
	.net_name = GPIO_NET_NAME(node), \
	.port_name = DT_GPIO_LABEL(DT_NODELABEL(node), gpios), \
	.pin = DT_GPIO_PIN(DT_NODELABEL(node), gpios), \
	.flags = DT_GPIO_FLAGS(DT_NODELABEL(node), gpios), \
	.port = DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(DT_NODELABEL(node), gpios, 0))

/* Check if the GPIO is present */
#define POWER_SEQ_GPIO_PRESENT(node) \
	DT_NODE_HAS_STATUS(DT_NODELABEL(node), okay)

/*
 * @brief Create power sequencing thread
 *
 * @param p1 Thread sleep time in ms
 * TODO: Add details about inputs
 */
void pwrseq_thread(void *p1, void *p2, void *p3);
int power_signal_is_asserted(enum power_signal signal);

extern struct gpio_config power_seq_gpios[];
extern const int power_seq_gpios_count;
extern enum power_states_ndsx chipset_pwr_sm_run(
				enum power_states_ndsx curr_state);
extern void init_chipset_pwr_seq_state(void);
extern void all_sig_pass_thru_handler(void);
extern void common_rsmrst_pass_thru_handler(void);
extern void pwr_sm_set_state(enum power_states_ndsx new_state);
#endif /* __X86_NON_DSX_COMMON_H__ */
