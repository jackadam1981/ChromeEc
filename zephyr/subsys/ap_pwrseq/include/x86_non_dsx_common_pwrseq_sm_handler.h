/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_COMMON_H__
#define __X86_NON_DSX_COMMON_H__

#include <x86_common_pwrseq.h>
#include <zephyr/types.h>

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

/* GPIO interrupt */
#define POWER_SEQ_INTR_GPIO(node) \
	.net_name = GPIO_NET_NAME(node), \
	.intr_flags = GPIO_INT_EDGE_BOTH

#define DT_DRV_COMPAT ap_pwrseq

#define POWER_SIGNAL_ACTIVE_STATE BIT(0)
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
/* TODO: Add runtime flag */
extern const struct power_signal_gpio_info power_signal_gpio_list[];
extern const struct power_signal_vw_info power_signal_vw_list[];

/*
 * @brief Create power sequencing thread
 *
 * @param p1 Thread sleep time in ms
 * TODO: Add details about inputs
 */
void pwrseq_thread(void *p1, void *p2, void *p3);
void power_update_signals(void);

extern struct gpio_config power_seq_gpios[];
extern struct gpio_interrupt_config power_seq_intr_gpios[];
extern const int power_seq_gpios_count;
extern const int power_seq_intr_gpios_count;
extern const int power_signal_gpio_count;
extern const int power_signal_vw_count;
extern enum power_states_ndsx chipset_pwr_sm_run(
				enum power_states_ndsx curr_state);
extern void chipset_force_shutdown(enum chipset_shutdown_reason reason);
extern void chipset_reset(enum chipset_shutdown_reason reason);
extern void init_chipset_pwr_seq_state(void);
extern void all_sig_pass_thru_handler(void);
extern void common_rsmrst_pass_thru_handler(void);
extern void pwr_sm_set_state(enum power_states_ndsx new_state);
#endif /* __X86_NON_DSX_COMMON_H__ */
