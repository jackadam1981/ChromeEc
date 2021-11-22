/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <devicetree/gpio.h>
#include <drivers/espi.h>
#include <x86_non_dsx_espi.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include <zephyr.h>
#include <string.h>
#include <logging/log.h>
#include <shell/shell.h>

LOG_MODULE_REGISTER(ap_pwrseq, 4);

static K_KERNEL_STACK_DEFINE(pwrseq_thread_stack, 1024);
static struct k_thread pwrseq_thread_data;
k_tid_t pwrseq_thread_id;
struct power_seq_context pwrseq_ctx;

const struct common_pwrseq_config com_cfg = {
	.pch_dsw_pwrok_delay_ms = DT_INST_PROP(0, dsw_pwrok_delay),
	.pch_pm_pwrbtn_delay_ms =  DT_INST_PROP(0, pm_pwrbtn_delay),
	.pch_rsmrst_delay_ms = DT_INST_PROP(0, rsmrst_delay),
	.wait_signal_timeout_ms = DT_INST_PROP(0, wait_signal_timeout),
};

/**
 * @brief power_state names for debug
 */
const char pwrsm_dbg[][25] = {
	[SYS_POWER_STATE_G3] = "STATE_G3",
	[SYS_POWER_STATE_S5] = "STATE_S5",
	[SYS_POWER_STATE_S4] = "STATE_S4",
	[SYS_POWER_STATE_S3] = "STATE_S3",
	[SYS_POWER_STATE_S0] = "STATE_S0",
	[SYS_POWER_STATE_G3S5] = "STATE_G3S5",
	[SYS_POWER_STATE_S5S4] = "STATE_S5S4",
	[SYS_POWER_STATE_S4S3] = "STATE_S4S3",
	[SYS_POWER_STATE_S3S0] = "STATE_S3S0",
	[SYS_POWER_STATE_S5G3] = "STATE_S5G3",
	[SYS_POWER_STATE_S4S5] = "STATE_S4S5",
	[SYS_POWER_STATE_S3S4] = "STATE_S3S4",
	[SYS_POWER_STATE_S0S3] = "STATE_S0S3",
};

const struct gpio_config *get_gpio_config_from_net_name(const char *net_name)
{
	const struct gpio_config *gpio;
	int i;

	for (i = 0; i < power_seq_gpios_count; i++) {
		gpio = &power_seq_gpios[i];
		if (!strcmp(gpio->net_name, net_name))
			return gpio;
	}

	LOG_ERR("Failed to find GPIO %s", net_name);
	return NULL;
}

int gpio_get_lvl(const char *net_name)
{
	const struct gpio_config *gpio =
		get_gpio_config_from_net_name(net_name);

	if (gpio)
		return gpio_pin_get_raw(gpio->port, gpio->pin);

	return 0;
}

void gpio_set_lvl(const char *net_name, int val)
{
	const struct gpio_config *gpio =
		get_gpio_config_from_net_name(net_name);

	if (gpio) {
		if (gpio_pin_set_raw(gpio->port, gpio->pin, val))
			LOG_ERR("Failed to set GPIO %s", net_name);
	}
}

void throttle_ap_prochot_input_interrupt(void)
{
	/* TODO: Add handling */
}

void power_signal_interrupt(void)
{
	/* TODO: Add handling */
}

static int check_power_rails_enabled(void)
{
	int out = 1;

#if POWER_SEQ_GPIO_PRESENT(EC_VR_EN_PP3300_A)
	out &= gpio_get_lvl(GPIO_NET_NAME(EC_VR_EN_PP3300_A));
#endif
#if POWER_SEQ_GPIO_PRESENT(EC_VR_EN_PP5000_A)
	out &= gpio_get_lvl(GPIO_NET_NAME(EC_VR_EN_PP5000_A));
#endif
#if POWER_SEQ_GPIO_PRESENT(VR_EC_DSW_PWROK)
	out &= gpio_get_lvl(GPIO_NET_NAME(VR_EC_DSW_PWROK));
#endif
	return out;
}

static void pwrseq_gpio_init(void)
{
	struct gpio_config *gpio;
	int i, ret = 0;

	for (i = 0; i < power_seq_gpios_count; i++) {
		gpio = &power_seq_gpios[i];

		LOG_INF("Configuring GPIO: net_name=%s, port_name=%s "
			"pin=0x%x, flag=0x%x",
			gpio->net_name, gpio->port_name,
			gpio->pin, gpio->flags);
		/* Get GPIO binding */
		if (!device_is_ready(gpio->port)) {
			LOG_DBG("gpio device not ready error\n");
			ret = -EINVAL;
			break;
		}

		/* Configure the GPIO */
		ret = gpio_pin_configure(gpio->port, gpio->pin, gpio->flags);
		if (ret != 0) {
			LOG_ERR("pin config failure %s", gpio->net_name);
			break;
		}
	}

	if (!ret)
		LOG_INF("Configuring GPIO complete");
	else
		LOG_ERR("Configure GPIO fail, err=%d: net_name=%s "
			"port_name=%s, pin=0x%x, flag=0x%x",
			ret, gpio->net_name, gpio->port_name,
			gpio->pin, gpio->flags);
}

enum power_states_ndsx pwr_sm_get_state(void)
{
	return pwrseq_ctx.power_state;
}

void pwr_sm_set_state(enum power_states_ndsx new_state)
{
	/* Add locking mechanism if multiple thread can update it */
	LOG_DBG("Power state: %s --> %s\n", pwrsm_dbg[pwrseq_ctx.power_state],
					pwrsm_dbg[new_state]);
	pwrseq_ctx.power_state = new_state;
}

/* Check RSMRST is fine to move from S5 to higher state */
int check_rsmrst_ok(void)
{
	/* TODO: Check if this is still intact*/
	return gpio_get_lvl(GPIO_NET_NAME(VR_PG_EC_RSMRST_ODL));
}

int check_pch_out_of_suspend(void)
{
	return gpio_get_lvl(GPIO_NET_NAME(PCH_EC_SLP_SUS_L));
}

void pwr_signal_pass_thru_handler(const char *in_signal,
			const char *out_signal, uint32_t delay_ms)
{
	int in_sig_val = gpio_get_lvl(in_signal);

	if (in_sig_val != gpio_get_lvl(out_signal)) {
		if (in_sig_val)
			k_msleep(delay_ms);

		gpio_set_lvl(out_signal, in_sig_val);
	}
}



/* Handling RSMRST signal is mostly common across x86 chipsets */
__attribute__((weak)) void rsmrst_pass_thru_handler(void)
{
	/* Handle RSMRST passthrough */
	/* TODO: Add additional conditions for RSMRST handling */
	pwr_signal_pass_thru_handler(GPIO_NET_NAME(VR_PG_EC_RSMRST_ODL),
			GPIO_NET_NAME(EC_PCH_RSMRST_L),
			com_cfg.pch_rsmrst_delay_ms);
}


/* TODO:
 * Add power down sequence
 * Add power signal monitoring
 * Add logic to suspend and resume the thread
 */
static int common_pwr_sm_run(int state)
{
	switch (state) {
	case SYS_POWER_STATE_G3:
		/* Nothing to do */
		break;

	case SYS_POWER_STATE_G3S5:
		/* TODO: Check if we are good to move to S5*/
		if (check_pch_out_of_suspend())
			return SYS_POWER_STATE_S5;
		break;

	case SYS_POWER_STATE_S5:
		/* If A-rails are stable move to higher state */
		if (check_power_rails_enabled() && check_rsmrst_ok()) {
			/* rsmrst is intact */
			rsmrst_pass_thru_handler();
			return SYS_POWER_STATE_S5S4;
		}
		return SYS_POWER_STATE_S5G3;

	case SYS_POWER_STATE_S5G3:
		chipset_force_shutdown(CHIPSET_SHUTDOWN_G3);
		return SYS_POWER_STATE_G3;

	case SYS_POWER_STATE_S5S4:
		/* Check if the PCH has come out of suspend state */
		if (check_rsmrst_ok()) {
			LOG_DBG("RSMRST is ok");
			return SYS_POWER_STATE_S4;
		}
		LOG_DBG("RSMRST is not ok");
		return SYS_POWER_STATE_S5;

	case SYS_POWER_STATE_S4:
		return SYS_POWER_STATE_S3;

	case SYS_POWER_STATE_S3:
		/* AP is out of suspend to RAM */
		if (gpio_get_lvl(GPIO_NET_NAME(PCH_EC_SLP_S3_L)))
			return SYS_POWER_STATE_S3S0;
		break;

	case SYS_POWER_STATE_S3S0:
		/* All the power rails must be stable */
		if (gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD)))
			return SYS_POWER_STATE_S0;
		break;

	case SYS_POWER_STATE_S0:
		/* Stay in S0 */
		break;

	case SYS_POWER_STATE_S4S5:
	case SYS_POWER_STATE_S3S4:
	case SYS_POWER_STATE_S0S3:
		break;

	default:
		break;
	}

	return state;
}
/* Console commands */

static int powerinfo_handler(const struct shell *shell, size_t argc,
							char **argv)
{
	int state;

	state = pwr_sm_get_state();
	shell_fprintf(shell, SHELL_INFO, "Power state = %d (%s)\n",
					state, pwrsm_dbg[state]);
	return 0;
}

SHELL_CMD_REGISTER(powerinfo, NULL, NULL, powerinfo_handler);

static int apshutdown_handler(const struct shell *shell, size_t argc,
							char **argv)
{
	if (pwr_sm_get_state() != SYS_POWER_STATE_G3) {
		chipset_force_shutdown(CHIPSET_SHUTDOWN_CONSOLE_CMD);
		LOG_INF("Shutdown activated\n");
		pwr_sm_set_state(SYS_POWER_STATE_G3);
	}

	return 0;
}

SHELL_CMD_REGISTER(apshutdown, NULL, NULL, apshutdown_handler);

static int apreset_handler(const struct shell *shell, size_t argc,
							char **argv)
{
	LOG_DBG("Issuing AP reset\n");
	chipset_reset(CHIPSET_SHUTDOWN_CONSOLE_CMD);

	return 0;
}

SHELL_CMD_REGISTER(apreset, NULL, NULL, apreset_handler);

static int powerup_handler(const struct shell *shell, size_t argc,
							char **argv)
{
	LOG_DBG("Powering up AP...\n");
	pwr_sm_set_state(SYS_POWER_STATE_G3S5);
	return 0;
}

SHELL_CMD_REGISTER(powerup, NULL, NULL, powerup_handler);

/* End of console commands */

void pwrseq_loop_thread(void *p1, void *p2, void *p3)
{
	int32_t t_wait_ms = 10;
	enum power_states_ndsx curr_state, new_state;

	while (1) {
		curr_state = pwr_sm_get_state();
		/* Run chipset specific state machine */
		new_state = chipset_pwr_sm_run(curr_state);

		/*
		 * Run common power state machine
		 * if the state has changed in chipset state
		 * machine then skip running common state
		 * machine
		 */
		if (curr_state == new_state)
			new_state = common_pwr_sm_run(curr_state);

		if (curr_state != new_state)
			pwr_sm_set_state(new_state);

		k_msleep(t_wait_ms);
	}
}

static inline void create_pwrseq_thread(void)
{
	/* TODO: Configure parameters for delay to the thread */
	pwrseq_thread_id = k_thread_create(&pwrseq_thread_data,
			pwrseq_thread_stack,
			K_KERNEL_STACK_SIZEOF(pwrseq_thread_stack),
			(k_thread_entry_t)pwrseq_loop_thread,
			NULL, NULL, NULL,
			K_PRIO_COOP(8), 0, K_NO_WAIT);

	k_thread_name_set(&pwrseq_thread_data, "pwrseq_task");
}

void init_pwr_seq_state(void)
{
	/* TODO: Read from device tree */

	com_cfg.pch_rsmrst_delay_ms = 10;
	com_cfg.pch_pm_pwrbtn_delay_ms = 200;

	/* Delay value can be ovverriden by chipset */
	init_chipset_pwr_seq_state();

	pwr_sm_set_state(SYS_POWER_STATE_G3S5);
}

/* Initialize power sequence system state */
static int pwrseq_init()
{
	LOG_ERR("Pwrseq Init\n");

	/* Configure gpio from device tree */
	pwrseq_gpio_init();
	LOG_DBG("Done gpio init");
	/* Register espi handler */
	ndsx_espi_configure();
	/* TODO: Define initial state of power sequence */
	LOG_DBG("Init pwr seq state");
	init_pwr_seq_state();
	/* Create power sequence state handler core function thread */
	create_pwrseq_thread();
	return 0;
}

SYS_INIT(pwrseq_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
