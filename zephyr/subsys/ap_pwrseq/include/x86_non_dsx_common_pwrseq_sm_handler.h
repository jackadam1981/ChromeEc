/*
 * Copyright (c) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __X86_NON_DSX_H__
#define __X86_NON_DSX_H__

#include <zephyr/types.h>
#include <x86_common_pwrseq.h>

/* Power sequencing GPIOs */

#define PCH_EC_SLP_SUS_L	slpsus
#define PCH_EC_SLP_S0_L		slps0

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
 * and utilized in platform-specific manner */
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
#define GPIO_NET_NAME(node)	DT_LABEL(DT_NODELABEL(node))

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

/* Delay in ms for pass through signals */
#define POWER_EC_PCH_DSW_PWROK_DELAY_MS	100
#define POWER_EC_PCH_RSMRST_DELAY_MS	10
#define POWER_EC_PCH_SYS_PWROK_DELAY_MS	50
#define POWER_EC_VR_EN_VCCIN_DELAY_MS	5
#define POWER_EC_PCH_PM_PWRBTN_DELAY_MS	200

/*
 * Power signal flags:
 *
 * +-----------------+------------------------------------+
 * |     Bit #       |           Description              |
 * +------------------------------------------------------+
 * |       0         |      Active level (low/high)       |
 * +------------------------------------------------------+
 * |     1 : 32      |            Reserved                |
 * +-----------------+------------------------------------+
 */

#define POWER_SIGNAL_ACTIVE_STATE BIT(0)
#define POWER_SIGNAL_ACTIVE_LOW   (0 << 0)
#define POWER_SIGNAL_ACTIVE_HIGH  BIT(0)

/* Convert enum power_signal to a mask for signal functions */
#define POWER_SIGNAL_MASK(signal) (1 << (signal))

/* Input state flags. */
#define IN_PCH_SLP_S3_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S3)
#define IN_PCH_SLP_S4_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S4)
#define IN_PCH_SLP_S5_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S5)
#define IN_PCH_SLP_SUS_DEASSERTED POWER_SIGNAL_MASK(X86_SLP_SUS)
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
//TODO: Add runtime flag?
/*
#ifdef CONFIG_POWER_SIGNAL_RUNTIME_CONFIG
extern struct power_signal_info power_signal_list[];
#else
extern const struct power_signal_info power_signal_list[];
#endif
*/
extern const struct power_signal_info power_signal_list[];
extern const struct power_signal_vw_info power_signal_vw_list[];

void espi_bus_reset(void);

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

extern enum power_states_ndsx chipset_pwr_sm_run(enum power_states_ndsx curr_state);
extern void chipset_force_shutdown(enum chipset_shutdown_reason reason);
extern void chipset_reset(enum chipset_shutdown_reason reason);
extern void all_sig_pass_thru_handler(void);
extern void common_rsmrst_pass_thru_handler(void);
extern void pwr_sm_set_state(enum power_states_ndsx new_state);
#endif /* __X86_NON_DSX_H__ */
