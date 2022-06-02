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
#include "emul/emul_power_signals.h"
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
	struct k_work_delayable d_work;
	int assert_value;
	int assert_delay_ms;
	int deassert_value;
	int deassert_delay_ms;
	bool retain;
	bool invert;
	int value;
};

struct power_signal_emul_output {
	enum power_signal signal;
	enum power_signal_emul_source source;
	struct gpio_dt_spec spec;
	struct gpio_callback cb;
	struct power_signal_emul_input *in;
	int in_count;
	int assert_value;
	bool retain;
	int value;
};

struct ap_power_test_platform {
	char *name_id;
	int emul_signals_count;
	struct power_signal_emul_output *emul_signals;
};

#define AP_PWR_SIGNAL_EMUL_GET_SOURCE(inst)                                   \
	COND_CODE_1(DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_gpio),           \
		(PWR_SIG_EMUL_SRC_GPIO),                                      \
	(COND_CODE_1(DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_vw),            \
		(PWR_SIG_EMUL_SRC_VW),                                        \
	(COND_CODE_1(DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_external),      \
		(PWR_SIG_EMUL_SRC_EXT),                                       \
	(PWR_SIG_EMUL_SRC_ADC))))))

#define AP_PWR_SIGNAL_INPUT_INIT_DECL(inst)                                   \
	{                                                                     \
		.signal = PWR_SIGNAL_ENUM(DT_PROP(inst, input_signal)),       \
		.source = AP_PWR_SIGNAL_EMUL_GET_SOURCE(DT_PROP(inst,         \
			input_signal)),                                       \
		.assert_value = DT_PROP(inst, assert_value),                  \
		.assert_delay_ms = DT_PROP(inst, assert_delay_ms),            \
		.deassert_value = DT_PROP(inst, deassert_value),              \
		.deassert_delay_ms = DT_PROP(inst, deassert_delay_ms),        \
		.value = DT_PROP_OR(inst, init_value, 0),                     \
		.retain = !DT_NODE_HAS_PROP(inst, init_value),                \
		.invert = DT_PROP(inst, invert_value),                        \
		COND_CODE_1(DT_NODE_HAS_COMPAT(DT_PROP(inst, input_signal),   \
			intel_ap_pwrseq_gpio),                                \
			(.spec = GPIO_DT_SPEC_GET(DT_PROP(inst, input_signal),\
			 gpios)), ())                                         \
	},

#define AP_PWR_SIGNAL_EACH_INPUT_INIT_DECL(inst)                              \
	static struct power_signal_emul_input input_##inst[] = {              \
		DT_FOREACH_CHILD_STATUS_OKAY(inst,                            \
			AP_PWR_SIGNAL_INPUT_INIT_DECL)                        \
	};

DT_FOREACH_STATUS_OKAY(ap_pwr_signal_emul, AP_PWR_SIGNAL_EACH_INPUT_INIT_DECL)

#define AP_PWR_SIGNAL_INIT(inst)                                              \
	{                                                                     \
		.signal = PWR_SIGNAL_ENUM(DT_PROP(inst, output_signal)),      \
		.source = AP_PWR_SIGNAL_EMUL_GET_SOURCE(                      \
			DT_PROP(inst, output_signal)),                        \
		COND_CODE_1(DT_NODE_HAS_COMPAT(DT_PROP(inst, output_signal),  \
			intel_ap_pwrseq_gpio),                                \
			(.spec = GPIO_DT_SPEC_GET(                            \
			DT_PROP(inst, output_signal), gpios),), ())           \
		.in = input_##inst,                                           \
		.in_count = ARRAY_SIZE(input_##inst),                         \
		.assert_value = DT_PROP(inst, assert_value),                  \
		.value = DT_PROP_OR(inst, init_value, 0),                     \
		.retain = !DT_NODE_HAS_PROP(inst, init_value),                \
	},

#define AP_PWR_SIGNAL_INIT_2(inst) AP_PWR_SIGNAL_INIT(inst)

#define AP_PWR_TEST_PLATFORM_POWER_IO_SIGNALS(inst, prop, idx)                \
		AP_PWR_SIGNAL_INIT_2(DT_PHANDLE_BY_IDX(inst, prop, idx))

#define AP_PWR_TEST_PLATFORM_POWER_SIGNALS(inst)                              \
	static struct power_signal_emul_output io_signals_##inst[] = {        \
		DT_FOREACH_PROP_ELEM(inst, io_signals,                        \
			AP_PWR_TEST_PLATFORM_POWER_IO_SIGNALS)                \
	};

DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform, AP_PWR_TEST_PLATFORM_POWER_SIGNALS)

#define AP_PWR_TEST_PLATFORM(inst)                                            \
	[DT_STRING_TOKEN(inst, name_id)] = {                                  \
		.name_id = DT_PROP(inst, name_id),                            \
		.emul_signals_count = DT_PROP_LEN(inst, io_signals),          \
		.emul_signals = io_signals_##inst,                            \
	},

static struct ap_power_test_platform test_platforms[TEST_ID_COUNT] = {
DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform, AP_PWR_TEST_PLATFORM)
};

static struct ap_power_test_platform *cur_test_platform;

static void power_signal_emul_set_in_value(struct power_signal_emul_input
					   *in_signal, int value)
{
	int ret = 0;

	switch(in_signal->source) {
	case PWR_SIG_EMUL_SRC_GPIO:
		ret = gpio_emul_input_set(in_signal->spec.port,
			in_signal->spec.pin, !!value);
		break;
	case PWR_SIG_EMUL_SRC_EXT:
		ret = power_signal_set(in_signal->signal, value);
		break;
	default:
		power_signal_interrupt(in_signal->signal, in_signal->value);
		break;
	}
	zassert_true((ret == 0), "Setting input signal value");
}

static int power_signal_emul_get_in_value(struct power_signal_emul_input
					   *in_signal)
{
	int ret;

	switch(in_signal->source) {
	case PWR_SIG_EMUL_SRC_GPIO:
		ret = gpio_pin_get_raw(in_signal->spec.port,
			in_signal->spec.pin);
		break;
	case PWR_SIG_EMUL_SRC_VW:
	case PWR_SIG_EMUL_SRC_EXT:
		ret = power_signal_get(in_signal->signal);
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int power_signal_emul_get_out_value(struct power_signal_emul_output
					   *out_signal)
{
	int ret_val;
	int ret;

	switch(out_signal->source) {
	case PWR_SIG_EMUL_SRC_GPIO:
		ret = gpio_emul_output_get_masked(out_signal->spec.port,
						  BIT(out_signal->spec.pin),
						  (gpio_port_value_t*) &ret_val);
		zassert_true((ret == 0), "Getting output signal value");
		ret_val = !!ret_val;
		break;
	default:
		ret_val = 0;
		break;
	}
	return ret_val;
}

static void emul_power_signal_gpio_interrupt(const struct device *port,
				 struct gpio_callback *cb,
				 gpio_port_pins_t pins)
{
	struct power_signal_emul_output *out_signal = CONTAINER_OF(cb,
				struct power_signal_emul_output, cb);
	int out_value;
	int delay;
	int new_value;

	out_value = power_signal_emul_get_out_value(out_signal);
	for (int i = 0; i < out_signal->in_count; i++) {
		struct power_signal_emul_input *in_signal = &out_signal->in[i];

		new_value = (out_value == out_signal->assert_value) ^
			in_signal->invert ? in_signal->assert_value :
			in_signal->deassert_value;

		//TODO: Is this OK?
		if (new_value != in_signal->value) {
			delay = (out_value == out_signal->assert_value) ?
				in_signal->assert_delay_ms :
				in_signal->deassert_delay_ms;

			in_signal->value = new_value;

			k_work_schedule(&out_signal->in[i].d_work, K_MSEC(delay));
		}
	}
}

static void emul_signal_work_hanlder(struct k_work *work)
{
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct power_signal_emul_input *in_signal = CONTAINER_OF(d_work,
				struct power_signal_emul_input, d_work);

	power_signal_emul_set_in_value(in_signal, in_signal->value);
}

int power_signal_emul_load(enum test_platforms_id test_id)
{
	if (test_id >= TEST_ID_COUNT) {
		return -EINVAL;
	}

	if (cur_test_platform) {
		return -EBUSY;
	}

	cur_test_platform = &test_platforms[test_id];
	for(int i = 0; i < cur_test_platform->emul_signals_count; i++) {
		struct power_signal_emul_output *emul_signal =
				&cur_test_platform->emul_signals[i];

		for (int in_count = 0; in_count < emul_signal->in_count;
		     in_count++) {
			struct power_signal_emul_input *in_signal =
						&emul_signal->in[in_count];
			if (in_signal->retain) {
				in_signal->value =
					power_signal_emul_get_in_value(in_signal);
			} else {
				/* Not retaining previous value, override */
				power_signal_emul_set_in_value(in_signal,
							in_signal->value);
			}
			k_work_init_delayable(&in_signal->d_work,
					      emul_signal_work_hanlder);
		}

		if (emul_signal->retain) {
			emul_signal->value =
				power_signal_emul_get_out_value(emul_signal);
		} else {
			/* Not retaining previous value, override */
			power_signal_set(emul_signal->signal,
					 emul_signal->value);
		}
		if (emul_signal->source == PWR_SIG_EMUL_SRC_GPIO) {
			gpio_init_callback(&emul_signal->cb,
				   emul_power_signal_gpio_interrupt,
				   BIT(emul_signal->spec.pin));

			gpio_add_callback(emul_signal->spec.port,
					  &emul_signal->cb);

			gpio_pin_interrupt_configure_dt(&emul_signal->spec,
						GPIO_INT_EDGE_BOTH);
		}
	}
	return 0;
}

int power_signal_emul_unload(void)
{
	if (!cur_test_platform) {
		return -EINVAL;
	}

	for(int i = 0; i < cur_test_platform->emul_signals_count; i++) {
		struct power_signal_emul_output *emul_signal =
				&cur_test_platform->emul_signals[i];
		if (emul_signal->source != PWR_SIG_EMUL_SRC_GPIO) {
			continue;
		}

		for (int in_count = 0; in_count < emul_signal->in_count;
		     in_count++) {
			static struct k_work_sync work_sync;
			struct power_signal_emul_input *in_signal =
						&emul_signal->in[in_count];

			k_work_cancel_delayable_sync(&in_signal->d_work,
						     &work_sync);
		}
		gpio_pin_interrupt_configure_dt(&emul_signal->spec,
						GPIO_INT_DISABLE);
		gpio_remove_callback(emul_signal->spec.port, &emul_signal->cb);
	}
	cur_test_platform = NULL;
	return 0;
}
