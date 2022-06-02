/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/espi_emul.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/zephyr.h>
#include <ztest.h>

#include "ap_power/ap_power.h"
#include "ap_power/ap_power_events.h"
#include "chipset.h"
#include "power_signals.h"
#include "test_state.h"

static const struct device *emul_port;

/* Input signals */
#define INIT_IN_GPIO_SPEC(id)	\
	COND_CODE_1(DT_PROP(id, output), \
		(),(GPIO_DT_SPEC_GET(id, gpios),))

const static struct gpio_dt_spec in_spec[] = {
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_gpio, INIT_IN_GPIO_SPEC)
};

#define INIT_IN_POWER_SIGNAL(id)	\
	COND_CODE_1(DT_PROP(id, output), \
		(),(PWR_SIGNAL_ENUM(id),))

const enum power_signal in_power_signal[] = {
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_gpio, INIT_IN_POWER_SIGNAL)
};

/* Output signals */
#define INIT_OUT_GPIO_SPEC(id)	\
	COND_CODE_1(DT_PROP(id, output), \
		(GPIO_DT_SPEC_GET(id, gpios),),())

const static struct gpio_dt_spec out_spec[] = {
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_gpio, INIT_OUT_GPIO_SPEC)
};

#define INIT_OUT_POWER_SIGNAL(id)	\
	COND_CODE_1(DT_PROP(id, output), \
		(PWR_SIGNAL_ENUM(id),),())

static const uint8_t out_power_signal[] = {
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_gpio, INIT_OUT_POWER_SIGNAL)
};

struct gpio_callback out_gpio_cb[ARRAY_SIZE(out_power_signal)];

static struct k_delayed_work out_work[ARRAY_SIZE(out_power_signal)];

int emul_signal_get_signal_id(enum power_signal signal)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(in_power_signal); i++) {
		if (in_power_signal[i] == signal) {
			return i;
		}
	}
	return -1;
}

static void emul_signal_work_hanlder(struct k_work *work)
{
	uint8_t work_id = k_work_delayable_from_work(work) - &out_work->work;
	int in_signal;
	int value;

	switch(out_power_signal[work_id]) {
	case PWR_EN_PP3300_A:
		power_signal_set(PWR_DSW_PWROK, 1);
		power_signal_interrupt(PWR_DSW_PWROK, 1);
		return;
	case PWR_EN_PP5000_A:
		in_signal = emul_signal_get_signal_id(PWR_RSMRST);
		value = 1;
		break;
	case PWR_EC_SOC_DSW_PWROK:
		in_signal = emul_signal_get_signal_id(PWR_SLP_SUS);
		value = 1;
		break;
	case PWR_EC_PCH_RSMRST:
		power_signal_interrupt(PWR_SLP_S5, 0);
		power_signal_interrupt(PWR_SLP_S4, 0);
		power_signal_interrupt(PWR_SLP_S3, 0);
		power_signal_interrupt(PWR_SLP_S0, 0);
		power_signal_set(PWR_ALL_SYS_PWRGD, 1);
		power_signal_set(PWR_PG_PP1P05, 1);
		return;
	default:
		return;
	}

	if (in_signal < 0) {
		return;
	}
	gpio_emul_input_set(in_spec[in_signal].port, in_spec[in_signal].pin, value);
	power_signal_interrupt(in_signal, value);
}

bool flag;

static void emul_ev_handler(struct ap_power_ev_callback *callback,
		       struct ap_power_ev_data data)
{
	flag = 1;
}

ZTEST(ap_pwrseq, test_power_up)
{
	struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb, emul_ev_handler, AP_POWER_RESUME);
	ap_power_ev_add_callback(&cb);

	chipset_exit_hard_off();

	k_msleep(1000);
	ap_power_ev_remove_callback(&cb);

}

/**
 * @brief Test Suite: Verifies power signal functionality.
 */

static void emul_power_signal_gpio_interrupt(const struct device *port,
				 struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	uint8_t index = cb - out_gpio_cb;

	switch(out_power_signal[index]) {
	case PWR_EN_PP3300_A:
		k_delayed_work_submit(&out_work[index], K_MSEC(10));
		break;

	case PWR_EN_PP5000_A:
		k_delayed_work_submit(&out_work[index], K_MSEC(40));
		break;

	case PWR_EC_SOC_DSW_PWROK:
		k_delayed_work_submit(&out_work[index], K_MSEC(20));
		break;

	case PWR_EC_PCH_RSMRST:
		k_delayed_work_submit(&out_work[index], K_MSEC(15));
		break;
	default:
		break;
	}
}

static void *set_initial_signals_state(void)
{
	emul_port = device_get_binding("GPIO_0");
	for(int i = 0; i < ARRAY_SIZE(out_spec); i++) {
		gpio_init_callback(&out_gpio_cb[i],
			   emul_power_signal_gpio_interrupt,
			   BIT(out_spec[i].pin));
		gpio_add_callback(out_spec[i].port, &out_gpio_cb[i]);
		gpio_pin_interrupt_configure_dt(&out_spec[i], GPIO_INT_EDGE_BOTH);

		k_delayed_work_init(&out_work[i], emul_signal_work_hanlder);

	}
	return NULL;
}

ZTEST_SUITE(ap_pwrseq, ap_power_predicate_post_main,
	    set_initial_signals_state, NULL , NULL, NULL);
