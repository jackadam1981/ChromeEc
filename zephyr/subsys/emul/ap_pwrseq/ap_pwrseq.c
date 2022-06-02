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

enum power_signal_emul_source {
	PWR_SIG_EMUL_SRC_GPIO,
	PWR_SIG_EMUL_SRC_VW,
	PWR_SIG_EMUL_SRC_EXT,
	PWR_SIG_EMUL_SRC_ADC,
};

struct power_signal_emul_input {
	enum power_signal signal;
	enum power_signal_emul_source source;
	struct gpio_dt_spec spec;
	struct k_delayed_work d_work;
	int assert_value;
	int assert_delay_ms;
	int deassert_value;
	int deassert_delay_ms;
	int value;
};

struct power_signal_emul_output {
	enum power_signal signal;
	enum power_signal_emul_source source;
	struct gpio_dt_spec spec;
	struct gpio_callback cb;
	struct power_signal_emul_input *in;
	int in_count;
};

#define AP_PWR_SIGNAL_EMUL_GET_SOURCE(inst)     \
	COND_CODE_1(DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_gpio), (PWR_SIG_EMUL_SRC_GPIO), \
	(COND_CODE_1(DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_vw), (PWR_SIG_EMUL_SRC_VW),    \
	(COND_CODE_1(DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_external), (PWR_SIG_EMUL_SRC_EXT),\
	(PWR_SIG_EMUL_SRC_ADC))))))

#define AP_PWR_SIGNAL_INPUT_INIT_DECL(inst)  \
	{                    \
		.signal = PWR_SIGNAL_ENUM(DT_PROP(inst, input_signal)),      \
		.source = AP_PWR_SIGNAL_EMUL_GET_SOURCE(DT_PROP(inst, input_signal)), \
		.assert_value = DT_PROP(inst, assert_value),        \
		.assert_delay_ms = DT_PROP(inst, assert_delay_ms),   \
		.deassert_value = DT_PROP(inst, deassert_value),        \
		.deassert_delay_ms = DT_PROP(inst, deassert_delay_ms),   \
		COND_CODE_1(DT_NODE_HAS_COMPAT(DT_PROP(inst, input_signal), intel_ap_pwrseq_gpio),\
			(.spec = GPIO_DT_SPEC_GET(DT_PROP(inst, input_signal), gpios)), ()) \
	},

#define AP_PWR_SIGNAL_EACH_INPUT_INIT_DECL(inst)  \
	static struct power_signal_emul_input input_##inst[] = {    \
		DT_FOREACH_CHILD_STATUS_OKAY(inst, AP_PWR_SIGNAL_INPUT_INIT_DECL) \
	};

DT_FOREACH_STATUS_OKAY(intel_ap_pwr_signal_emul, AP_PWR_SIGNAL_EACH_INPUT_INIT_DECL)

#define AP_PWR_SIGNAL_INIT(inst)       \
	{                                   \
		.signal = PWR_SIGNAL_ENUM(DT_PROP(inst, output_signal)),\
		.source = AP_PWR_SIGNAL_EMUL_GET_SOURCE(DT_PROP(inst, output_signal)), \
		COND_CODE_1(DT_NODE_HAS_COMPAT(DT_PROP(inst, output_signal), intel_ap_pwrseq_gpio),\
			(.spec = GPIO_DT_SPEC_GET(DT_PROP(inst, output_signal), gpios),), ()) \
		.in = input_##inst, \
		.in_count = ARRAY_SIZE(input_##inst), \
	},

struct power_signal_emul_output emul_signals[] = {
DT_FOREACH_STATUS_OKAY(intel_ap_pwr_signal_emul, AP_PWR_SIGNAL_INIT)
};

int emul_power_signal_get_out_value(struct power_signal_emul_output *out_signal)
{
	gpio_port_value_t value = 0;
	int ret;

	switch(out_signal->source) {
	case PWR_SIG_EMUL_SRC_GPIO:
		ret = gpio_emul_output_get_masked(out_signal->spec.port,
			BIT(out_signal->spec.pin), &value);
		zassert_true((ret == 0), "Getting output signal value");
		break;
	default:
	break;
	}
	return !!value;
}

static void emul_power_signal_gpio_interrupt(const struct device *port,
				 struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	struct power_signal_emul_output *out_signal = CONTAINER_OF(cb,
				struct power_signal_emul_output, cb);
	int out_value;

	out_value = emul_power_signal_get_out_value(out_signal);
	printf("Getting out_value = %d <======\n", out_value);
	for (int i = 0; i < out_signal->in_count; i++) {
		int delay = out_value ? out_signal->in[i].assert_delay_ms :
				out_signal->in[i].deassert_delay_ms;
		out_signal->in[i].value = out_value;
		k_delayed_work_submit(&out_signal->in[i].d_work, K_MSEC(delay));
	}
}

static void print_in_signal(struct power_signal_emul_input *in_signal)
{
	printf("Input Signal: %s\n", power_signal_name(in_signal->signal));
}

static void emul_signal_work_hanlder(struct k_work *work)
{
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct power_signal_emul_input *in_signal = CONTAINER_OF(d_work,
				struct power_signal_emul_input, d_work);
	int value = in_signal->value ? in_signal->assert_value : in_signal->deassert_value ;
	print_in_signal(in_signal);
	printf("Setting in_value = %d <======\n", value);
	switch (in_signal->source) {
	case PWR_SIG_EMUL_SRC_GPIO:
		gpio_emul_input_set(in_signal->spec.port, in_signal->spec.pin, value);
		power_signal_interrupt(in_signal->signal, value);
		break;
	case PWR_SIG_EMUL_SRC_EXT:
		power_signal_set(in_signal->signal, value);
		break;
	default:
		power_signal_interrupt(in_signal->signal, value);
		break;
	}
}

void power_signal_emul_init(void)
{
	for(int i = 0; i < ARRAY_SIZE(emul_signals); i++) {
		if (emul_signals[i].source != PWR_SIG_EMUL_SRC_GPIO) {
			continue;
		}

		gpio_init_callback(&emul_signals[i].cb,
			   emul_power_signal_gpio_interrupt,
			   BIT(emul_signals[i].spec.pin));
		gpio_add_callback(emul_signals[i].spec.port, &emul_signals[i].cb);
		gpio_pin_interrupt_configure_dt(&emul_signals[i].spec, GPIO_INT_EDGE_BOTH);

		for (int in_count = 0; in_count < emul_signals[i].in_count; in_count++) {
			k_delayed_work_init(&emul_signals[i].in[in_count].d_work, emul_signal_work_hanlder);
		}
	}
}
