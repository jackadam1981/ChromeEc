/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <devicetree/gpio.h>
#include <drivers/espi.h>
#include <logging/log.h>
#include <stdlib.h>
#include <string.h>
#include <shell/shell.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include <x86_non_dsx_espi.h>
#include <zephyr.h>

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

/* S5 inactive timer*/
K_TIMER_DEFINE(s5_inactive_timer, NULL, NULL);

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

static const char *get_power_signal_net_name(enum power_signal signal)
{
	int i;

	for (i = 0; i < power_signal_gpio_count; i++) {
		if (signal == power_signal_gpio_list[i].power_sig)
			return power_signal_gpio_list[i].net_name;
	}
	return NULL;
}

static const struct gpio_interrupt_config *get_intr_config_from_power_signal(
	enum power_signal signal)
{
	const struct gpio_interrupt_config *intr;
	const char *net_name;
	int i;

	net_name = get_power_signal_net_name(signal);

	if (!net_name)
		return NULL;

	for (i = 0; i < power_seq_intr_gpios_count; i++) {
		intr = &power_seq_intr_gpios[i];
		if (!strcmp(intr->net_name, net_name))
			return intr;
	}
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

void chipset_set_exit_hard_off_flag(int enable)
{
	/* TODO: This could be accessed by multiple tasks
	 * at this point it is power button task
	 */
	pwrseq_ctx.want_g3_exit = enable;
}

void chipset_exit_hard_off(void)
{
	/*
	 * If not in the soft-off state, hard-off state, or headed there,
	 * nothing to do.
	 */
	if (pwrseq_ctx.power_state != SYS_POWER_STATE_G3 &&
	    pwrseq_ctx.power_state != SYS_POWER_STATE_S5G3 &&
	    pwrseq_ctx.power_state != SYS_POWER_STATE_S5)
		return;

	chipset_set_exit_hard_off_flag(1);
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

int power_signal_disable_interrupt(enum power_signal signal)
{
	const struct gpio_interrupt_config *intr;

	intr = get_intr_config_from_power_signal(signal);

	if (intr)
		return gpio_pin_interrupt_configure(intr->config->port,
						    intr->config->pin,
						    GPIO_INT_DISABLE);
	return -EINVAL;
}

int power_signal_enable_interrupt(enum power_signal signal)
{
	const struct gpio_interrupt_config *intr;

	intr = get_intr_config_from_power_signal(signal);
	if (intr)
		return gpio_pin_interrupt_configure(intr->config->port,
						    intr->config->pin,
						    intr->intr_flags);
	return -EINVAL;
}

int power_wait_mask_signals_timeout(uint32_t want, uint32_t mask, int timeout)
{
	int time_left = timeout;

	pwrseq_ctx.in_want = want;
	if (!mask)
		return 0;

	while (time_left--) {
		if ((pwrseq_ctx.in_signals & mask) != pwrseq_ctx.in_want)
			k_msleep(1);
		else
			return 0;
	}
	power_update_signals();
	return -ETIMEDOUT;
}

int power_wait_signals_timeout(uint32_t want, int timeout)
{
	return power_wait_mask_signals_timeout(want, want, timeout);
}

int power_wait_signals(uint32_t want)
{
	int ret = power_wait_signals_timeout(want,
				com_cfg.wait_signal_timeout_ms);

	if (ret == -ETIMEDOUT)
		LOG_INF("power timeout on input; wanted 0x%04x, got 0x%04x",
			want, pwrseq_ctx.in_signals & want);
	return ret;
}

__attribute__((weak)) int power_signal_gpio_is_asserted(
		const struct power_signal_gpio_info *s)
{
	return gpio_get_lvl(s->net_name) ==
		!!(s->flags & POWER_SIGNAL_ACTIVE_STATE);
}

int power_signal_vw_is_asserted(const struct power_signal_vw_info *vw)
{
	return vw_get_level(vw->vw_signal) ==
		!!(vw->flags & POWER_SIGNAL_ACTIVE_STATE);
}

int power_signal_is_asserted(enum power_signal signal)
{
	const struct power_signal_gpio_info *s = power_signal_gpio_list;
	const struct power_signal_vw_info *vw = power_signal_vw_list;
	int i;

	for (i = 0; i < power_signal_vw_count; i++, vw++) {
		if (signal == vw->power_sig)
			return power_signal_vw_is_asserted(vw);
	}

	for (i = 0; i < power_signal_gpio_count; i++, s++) {
		if (signal == s->power_sig)
			return power_signal_gpio_is_asserted(s);
	}

	return 0;
}

/**
 * Update input signals mask
 */
void power_update_signals(void)
{
	uint32_t inew = 0;
	const struct power_signal_gpio_info *s = power_signal_gpio_list;
	const struct power_signal_vw_info *vw = power_signal_vw_list;
	int i;

	for (i = 0; i < power_signal_gpio_count; i++, s++) {
		if (power_signal_gpio_is_asserted(s))
			inew |= BIT(s->power_sig);
	}

	for (i = 0; i < power_signal_vw_count; i++, vw++) {
		if (power_signal_vw_is_asserted(vw))
			inew |= BIT(vw->power_sig);
	}

	if ((pwrseq_ctx.in_signals & pwrseq_ctx.in_debug) !=
					(inew & pwrseq_ctx.in_debug))
		LOG_INF("power update 0x%04x->0x%04x",
					pwrseq_ctx.in_signals, inew);

	pwrseq_ctx.in_signals = inew;
}

uint32_t power_get_signals(void)
{
	return pwrseq_ctx.in_signals;
}

bool power_has_signals(uint32_t want)
{
	if ((pwrseq_ctx.in_signals & want) == want)
		return true;

	return false;
}

void power_signal_cb(const struct device *gpiodev,
			struct gpio_callback *cb,
			uint32_t pin)
{
	int i;
	const struct gpio_interrupt_config *intr_config = NULL;

	for (i = 0; i < power_seq_intr_gpios_count; i++) {
		if (gpiodev == power_seq_intr_gpios[i].config->port &&
			pin == BIT(power_seq_intr_gpios[i].config->pin)) {
			/* TODO: Monitor interrupt storm */
			intr_config = &power_seq_intr_gpios[i];
			break;
		}
	}

	if (!intr_config) {
		LOG_ERR("gpio int, can't find dev %p (pin %d)\n",
				gpiodev, pin);
		return;
	}

	power_update_signals();
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

	for (i = 0; i < power_seq_intr_gpios_count; i++) {
		const struct gpio_config *config;

		config = get_gpio_config_from_net_name(
				power_seq_intr_gpios[i].net_name);
		if (config == NULL) {
			LOG_ERR("Can't find GPIO %s device config",
				power_seq_intr_gpios[i].net_name);
			break;
		}

		power_seq_intr_gpios[i].config = config;

		/* Configure interrupt */
		gpio_init_callback(&power_seq_intr_gpios[i].intr_cb,
					power_signal_cb,
					BIT(config->pin));
		ret = gpio_add_callback(power_seq_intr_gpios[i].config->port,
			&power_seq_intr_gpios[i].intr_cb);

		if (!ret) {
			if (power_seq_intr_gpios[i].disable_at_boot)
				gpio_pin_interrupt_configure(
						config->port,
						config->pin,
						GPIO_INT_DISABLE);
			else
				gpio_pin_interrupt_configure(
					config->port,
					config->pin,
					power_seq_intr_gpios[i].intr_flags);
		} else {
			LOG_ERR("Failed GPIO interrupt callback i=%d ret=%d",
					i, ret);
		}
	}
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
	int ret;

	ret = power_wait_signals_timeout(
		IN_PCH_SLP_SUS_DEASSERTED, IN_PCH_SLP_SUS_WAIT_TIME_MS);
	if (ret == 0)
		return 1;
	return 0; /* timeout */
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
 * Add logic to suspend and resume the thread
 */
static int common_pwr_sm_run(int state)
{
	switch (state) {
	case SYS_POWER_STATE_G3:
		if (pwrseq_ctx.want_g3_exit) {
			chipset_set_exit_hard_off_flag(0);
			return SYS_POWER_STATE_G3S5;
		}

		break;

	case SYS_POWER_STATE_G3S5:
		if (power_wait_signals(IN_PGOOD_ALL_CORE))
			break;
		/*
		 * Now wait for SLP_SUS_L to go high based on tPCH32. If this
		 * signal doesn't go high within 250 msec then go back to G3.
		 */
		if (check_pch_out_of_suspend())
			return SYS_POWER_STATE_S5;

		return SYS_POWER_STATE_S5G3;

	case SYS_POWER_STATE_S5:
		/* In S5 make sure no more signal lost */
		/* If A-rails are stable then move to higher state */
		if (check_power_rails_enabled() && check_rsmrst_ok()) {
			/* rsmrst is intact */
			rsmrst_pass_thru_handler();

			if (!power_has_signals(IN_PCH_SLP_SUS_DEASSERTED)) {
				k_timer_stop(&s5_inactive_timer);
				return SYS_POWER_STATE_S5G3;
			}
			if (power_has_signals(IN_PCH_SLP_S5_DEASSERTED)) {
				k_timer_stop(&s5_inactive_timer);
				return SYS_POWER_STATE_S5S4;
			}
		}

		/* S5 inactivity timeout, go to S5G3 */
		if (pwrseq_ctx.s5_timeout_s == 0)
			return SYS_POWER_STATE_S5G3;
		else if (pwrseq_ctx.s5_timeout_s > 0) {
			if (k_timer_status_get(&s5_inactive_timer) > 0)
				/* Timer is expired */
				return SYS_POWER_STATE_S5G3;
			else if (k_timer_remaining_get(
						&s5_inactive_timer) == 0)
				/* Timer is not started or stopped */
				k_timer_start(&s5_inactive_timer,
					K_SECONDS(pwrseq_ctx.s5_timeout_s),
					K_NO_WAIT);
		}
		break;

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
		if (!power_has_signals(IN_PCH_SLP_S5_DEASSERTED))
			return SYS_POWER_STATE_S4S5;
		else if (power_has_signals(IN_PCH_SLP_S4_DEASSERTED))
			return SYS_POWER_STATE_S4S3;
		break;

	case SYS_POWER_STATE_S4S3:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown(CHIPSET_SHUTDOWN_POWERFAIL);
			return SYS_POWER_STATE_G3;
		}
		/* Call hooks now that rails are up */
		/* TODO: hook_notify(HOOK_CHIPSET_STARTUP); */

		/* TODO: S0ix
		 * Clearing the S0ix flag on the path to S0
		 * to handle any reset conditions.
		 */
		return SYS_POWER_STATE_S3;

	case SYS_POWER_STATE_S3:
		/* AP is out of suspend to RAM */
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away, go straight to S5 */
			chipset_force_shutdown(CHIPSET_SHUTDOWN_POWERFAIL);
			return SYS_POWER_STATE_G3;
		} else if (power_has_signals(IN_PCH_SLP_S3_DEASSERTED))
			return SYS_POWER_STATE_S3S0;
		else if (!power_has_signals(IN_PCH_SLP_S4_DEASSERTED))
			return SYS_POWER_STATE_S3S4;
		break;

	case SYS_POWER_STATE_S3S0:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			chipset_force_shutdown(CHIPSET_SHUTDOWN_POWERFAIL);
			return SYS_POWER_STATE_G3;
		}

		/* All the power rails must be stable */
		if (gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD)))
			return SYS_POWER_STATE_S0;
		break;

	case SYS_POWER_STATE_S0:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			chipset_force_shutdown(CHIPSET_SHUTDOWN_POWERFAIL);
			return SYS_POWER_STATE_G3;
		} else if (!power_has_signals(IN_PCH_SLP_S3_DEASSERTED))
			return SYS_POWER_STATE_S0S3;
		/* TODO: S0ix */

		break;

	case SYS_POWER_STATE_S4S5:
#if 0
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN);
		/* Disable wireless */
		wireless_set_state(WIRELESS_OFF);
		/* Call hooks after we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN_COMPLETE);
		/* Always enter into S5 state. The S5 state is required to
		 * correctly handle global resets which have a bit of delay
		 * while the SLP_Sx_L signals are asserted then deasserted.
		 */
		power_s5_up = 0;
#endif
		return SYS_POWER_STATE_S5;

	case SYS_POWER_STATE_S3S4:
		return SYS_POWER_STATE_S4;

	case SYS_POWER_STATE_S0S3:
		/* Call hooks before we remove power rails */
		/* TODO: hook_notify(HOOK_CHIPSET_SUSPEND); */
		return SYS_POWER_STATE_S3;

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

static const char *get_power_signal_name(enum power_signal signal)
{
	const struct power_signal_gpio_info *s = power_signal_gpio_list;
	const struct power_signal_vw_info *vw = power_signal_vw_list;
	int i;

	for (i = 0; i < power_signal_vw_count; i++, vw++) {
		if (signal == vw->power_sig)
			return vw->name;
	}

	for (i = 0; i < power_signal_gpio_count; i++, s++) {
		if (signal == s->power_sig)
			return s->name;
	}

	return NULL;
}

static int powerindebug_handler(const struct shell *shell, size_t argc,
							char **argv)
{
	int i;
	char *e;

	/* If one arg, set the mask */
	if (argc == 2) {
		int m = strtol(argv[1], &e, 0);

		if (*e)
			return -EINVAL;
		pwrseq_ctx.in_debug = m;
	}
	/* Print the mask */
	shell_fprintf(shell, SHELL_INFO, "power in:   0x%04x\n",
						pwrseq_ctx.in_signals);
	shell_fprintf(shell, SHELL_INFO, "debug mask: 0x%04x\n",
						pwrseq_ctx.in_debug);
	/* Print the decode */
	shell_fprintf(shell, SHELL_INFO, "bit meanings:\n");
	for (i = 0; i < POWER_SIGNAL_COUNT; i++) {
		int mask = 1 << i;

		shell_fprintf(shell, SHELL_INFO, "  0x%04x %d %s\n",
			mask, pwrseq_ctx.in_signals & mask ? 1 : 0,
			get_power_signal_name(i));
	}
	return 0;
};

SHELL_CMD_REGISTER(powerindebug, NULL,
	"[mask] Get/set power input debug mask", powerindebug_handler);

/* End of console commands */

void pwrseq_loop_thread(void *p1, void *p2, void *p3)
{
	int32_t t_wait_ms = 10;
	uint32_t this_in_signals;
	static uint32_t last_in_signals;
	static enum power_states_ndsx last_state;
	enum power_states_ndsx curr_state, new_state;

	while (1) {
		curr_state = pwr_sm_get_state();

		/*
		 * In order to prevent repeated console spam, only print the
		 * current power state if something has actually changed.  It's
		 * possible that one of the power signals goes away briefly and
		 * comes back by the time we update our pwrseq_ctx.in_signals.
		 */
		this_in_signals = pwrseq_ctx.in_signals;
		if (this_in_signals != last_in_signals ||
				curr_state != last_state) {
			LOG_INF("power state %d = %s, in 0x%04x",
				curr_state, pwrsm_dbg[curr_state],
				this_in_signals);
			last_in_signals = this_in_signals;
			last_state = curr_state;
		}

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
	pwrseq_ctx.s5_timeout_s = 10; /* Seconds */
	/* Delay value can be ovverriden by chipset */
	init_chipset_pwr_seq_state();

	chipset_set_exit_hard_off_flag(0);
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
