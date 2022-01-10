/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_H__
#define __X86_NON_DSX_COMMON_H__

#include <zephyr/types.h>
#include <x86_common_pwrseq.h>

/* Power sequencing GPIOs */

#define PCH_EC_SLP_SUS_L	gpio_slp_sus_l
#define PCH_EC_SLP_S0_L		gpio_slp_s0_l
#define PCH_EC_SLP_S3_L		gpio_slp_s3_l
#define PCH_EC_SLP_S4_L		gpio_slp_s4_l
#define PCH_EC_SLP_S5_L		gpio_slp_s5_l

/* DSW_PWROK is indication to PCH that 3P3V is stable */
#define VR_EC_DSW_PWROK		gpio_pg_ec_dsw_pwrok
#define EC_PCH_DSW_PWROK	gpio_ec_soc_dsw_pwrok

/* RSMRST is used for resetting primary power plane logic
 * When de-asserted, this signal is an indication that the
 * power wells are stable.
 */
#define VR_PG_EC_RSMRST_ODL	gpio_pg_ec_rsmrst_odl
#define EC_PCH_RSMRST_L		gpio_ec_pch_rsmrst_odl

/* Signal represents the power good for all the rest
 * of platform voltage rails.
 */
#define VR_EC_ALL_SYS_PWRGD	gpio_pg_ec_all_sys_pwrgd

#define PCH_PWROK		gpio_ec_soc_pch_pwrok_od
/* SYS_PWROK is a generic power good input to the PCH is driven
 * and utilized in platform-specific manner
 */
#define EC_PCH_SYS_PWROK	gpio_ec_soc_sys_pwrok

/* EC to PCH indication system request to sleep / wake */
#define EC_PCH_PWR_BTN_ODL	gpio_ec_pch_pwr_btn_odl

/* Enable 5V rails */
#define EC_VR_EN_PP5000_A	gpio_en_pp5000_s5
/* Enable 3.3V rails */
#define EC_VR_EN_PP3300_A	gpio_en_pp3300_s5

#define IMVP9_VRRDY_OD		gpio_imvp9_vrrdy_od

/* TODO: Move to chipset */
#define VCCST_PWRGD_OD		gpio_vccst_pwrgd_od

#define SYS_RESET_L		gpio_sys_rst_odl

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
