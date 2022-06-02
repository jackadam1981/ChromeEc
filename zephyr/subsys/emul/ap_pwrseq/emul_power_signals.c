/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/espi_emul.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include "ap_power/ap_power.h"
#include "ap_power/ap_power_events.h"
#include <zephyr/drivers/adc.h>
#include "chipset.h"
#include "emul/emul_power_signals.h"
#include "power_signals.h"
#include "test_state.h"

LOG_MODULE_REGISTER(emul_power_signal, CONFIG_EMUL_POWER_SIGNALS_LOG_LEVEL);

enum power_signal_emul_source {
	PWR_SIG_EMUL_SRC_GPIO,
	PWR_SIG_EMUL_SRC_VW,
	PWR_SIG_EMUL_SRC_EXT,
	PWR_SIG_EMUL_SRC_ADC,
};

union power_signal_emul_signal_spec {
	struct gpio_dt_spec gpio;
	struct adc_dt_spec adc;
};

struct power_signal_emul_signal_desc {
	const enum power_signal enum_id;
	const char *name;
	const enum power_signal_emul_source source;
	const union power_signal_emul_signal_spec spec;
};

struct power_signal_emul_input {
	struct power_signal_emul_signal_desc desc;
	const int assert_value;
	const int assert_delay_ms;
	const int deassert_value;
	const int deassert_delay_ms;
	const int init_value;
	const bool retain;
	const bool invert;
	struct k_work_delayable d_work;
	int value;
};

enum power_signal_edge {
	EDGE_ACTIVE_ON_ASSERT,
	EDGE_ACTIVE_ON_DEASSERT,
	EDGE_ACTIVE_ON_BOTH,
};

struct power_signal_emul_output {
	struct power_signal_emul_signal_desc desc;
	const int assert_value;
	const int init_value;
	const bool retain;
	const enum power_signal_edge edge;
	struct gpio_callback cb;
	int value;
};

struct power_signal_emul_node {
	char *name;
	struct power_signal_emul_output output;
	int inputs_count;
	struct power_signal_emul_input inputs[];
};

struct power_signal_emul_test_platform {
	char *name_id;
	int nodes_count;
	struct power_signal_emul_node **nodes;
};

#define EMUL_POWER_SIGNAL_GET_SOURCE(inst)                                    \
	COND_CODE_1(                                                          \
		DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_gpio),               \
		(PWR_SIG_EMUL_SRC_GPIO),                                      \
		(COND_CODE_1(                                                 \
			DT_NODE_HAS_COMPAT(inst, intel_ap_pwrseq_vw),         \
			(PWR_SIG_EMUL_SRC_VW),                                \
			(COND_CODE_1(DT_NODE_HAS_COMPAT(                      \
					     inst, intel_ap_pwrseq_external), \
				     (PWR_SIG_EMUL_SRC_EXT),                  \
				     (PWR_SIG_EMUL_SRC_ADC))))))

#define EMUL_POWER_SIGNAL_GET_SIGNAL_SPEC(inst, dir_signal)               \
	{                                                                 \
		COND_CODE_1(DT_NODE_HAS_COMPAT(DT_PROP(inst, dir_signal), \
					       intel_ap_pwrseq_gpio),     \
			    (.gpio = GPIO_DT_SPEC_GET(                    \
				     DT_PROP(inst, dir_signal), gpios)),  \
			    ())                                           \
	}

#define EMUL_POWER_SIGNAL_GET_SIGNAL(inst, dir)                             \
	{                                                                   \
		.enum_id = PWR_SIGNAL_ENUM(DT_PROP(inst, dir)),             \
		.name = DT_PROP(DT_PROP(inst, dir), enum_name),             \
		.source = EMUL_POWER_SIGNAL_GET_SOURCE(DT_PROP(inst, dir)), \
		.spec = EMUL_POWER_SIGNAL_GET_SIGNAL_SPEC(inst, dir),       \
	}

#define EMUL_POWER_SIGNAL_OUT_DEF(inst)                                    \
	{                                                                  \
		.desc = EMUL_POWER_SIGNAL_GET_SIGNAL(inst, output_signal), \
		.assert_value = DT_PROP(inst, assert_value),               \
		.init_value = DT_PROP_OR(inst, init_value, 0),             \
		.edge = DT_STRING_TOKEN(inst, edge),                       \
		.retain = !DT_NODE_HAS_PROP(inst, init_value),             \
	}

#define EMUL_POWER_SIGNAL_IN_DEF(inst)                                    \
	{                                                                 \
		.desc = EMUL_POWER_SIGNAL_GET_SIGNAL(inst, input_signal), \
		.assert_value = DT_PROP(inst, assert_value),              \
		.assert_delay_ms = DT_PROP(inst, assert_delay_ms),        \
		.deassert_value = DT_PROP(inst, deassert_value),          \
		.deassert_delay_ms = DT_PROP(inst, deassert_delay_ms),    \
		.init_value = DT_PROP_OR(inst, init_value, 0),            \
		.retain = !DT_NODE_HAS_PROP(inst, init_value),            \
		.invert = DT_PROP(inst, invert_value),                    \
	},

#define EMUL_POWER_SIGNAL_IN_ARRAY_DEF(inst)                                 \
	{                                                                    \
		DT_FOREACH_CHILD_STATUS_OKAY(inst, EMUL_POWER_SIGNAL_IN_DEF) \
	}

#define EMUL_POWER_SIGNAL_GET_INPUT_ENUM(inst) ENUM_##inst##_INPUT,

#define EMUL_POWER_SIGNAL_NODES_DEF(inst)                                      \
	enum {                                                                 \
		DT_FOREACH_CHILD_STATUS_OKAY(inst,                             \
					     EMUL_POWER_SIGNAL_GET_INPUT_ENUM) \
			inst##_INPUT_COUNT,                                    \
	};                                                                     \
	static struct power_signal_emul_node DT_CAT(inst, _node) = {           \
		.name = DT_NODE_FULL_NAME(inst),                               \
		.output = EMUL_POWER_SIGNAL_OUT_DEF(inst),                     \
		.inputs_count = inst##_INPUT_COUNT,                            \
		.inputs = EMUL_POWER_SIGNAL_IN_ARRAY_DEF(inst),                \
	};

DT_FOREACH_STATUS_OKAY(ap_pwr_signal_emul, EMUL_POWER_SIGNAL_NODES_DEF)

#define EMUL_POWER_SIGNAL_NODES_ARRAY_GET_NODES_REFS_WITH_COMMA(inst) \
	(&DT_CAT(inst, _node)),

#define EMUL_POWER_SIGNAL_NODES_ARRAY_GET_NODES_REFS(inst) \
	EMUL_POWER_SIGNAL_NODES_ARRAY_GET_NODES_REFS_WITH_COMMA(inst)

#define EMUL_POWER_SIGNAL_NODES_ARRAY_GET_IO_SIGNALS(inst, prop, idx) \
	EMUL_POWER_SIGNAL_NODES_ARRAY_GET_NODES_REFS(                 \
		DT_PHANDLE_BY_IDX(inst, prop, idx))

#define EMUL_POWER_SIGNAL_NODES_ARRAY_DEF(inst)                          \
	static struct power_signal_emul_node *DT_CAT(inst, _nodes)[] = { \
		DT_FOREACH_PROP_ELEM(                                    \
			inst, io_signals,                                \
			EMUL_POWER_SIGNAL_NODES_ARRAY_GET_IO_SIGNALS)    \
	};

DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform, EMUL_POWER_SIGNAL_NODES_ARRAY_DEF)

#define EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_NODES_REFS_ITEM(inst) \
	DT_CAT(inst, _nodes),

#define EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_NODES_REFS(inst) \
	EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_NODES_REFS_ITEM(inst)

#define EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_IO_SIGNALS(inst, prop, idx) \
	EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_NODES_REFS(                 \
		DT_PHANDLE_BY_IDX(inst, prop, idx))

#define EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_NODES(inst)                 \
	{                                                               \
		DT_FOREACH_PROP_ELEM(                                   \
			inst, io_signals,                               \
			EMUL_POWER_SIGNAL_TEST_PLATFORM_GET_IO_SIGNALS) \
	}

#define EMUL_POWER_SIGNAL_TEST_PLATFORM_DEF(inst)             \
	[DT_STRING_TOKEN(inst, name_id)] = {                  \
		.name_id = DT_PROP(inst, name_id),            \
		.nodes_count = DT_PROP_LEN(inst, io_signals), \
		.nodes = DT_CAT(inst, _nodes),                \
	},

static struct power_signal_emul_test_platform test_platforms[TEST_ID_COUNT] = {
	DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform,
			       EMUL_POWER_SIGNAL_TEST_PLATFORM_DEF)
};

static struct power_signal_emul_test_platform *cur_test_platform;

/*
 * Signal manipulation functions
 */
static void power_signal_emul_set_gpio_value(const struct gpio_dt_spec *spec,
					     int value)
{
	gpio_flags_t gpio_flags;
	int ret;

	ret = gpio_emul_flags_get(spec->port, spec->pin, &gpio_flags);
	zassert_true((ret == 0), "Getting GPIO flags!!");

	if (gpio_flags & GPIO_INPUT) {
		ret = gpio_emul_input_set(spec->port, spec->pin, value);
	} else if (gpio_flags & GPIO_OUTPUT) {
		ret = gpio_pin_set(spec->port, spec->pin, value);
	}
	zassert_true((ret == 0), "Setting GPIO value!!");
}

static void
power_signal_emul_set_value(struct power_signal_emul_signal_desc *desc,
			    int value)
{
	int ret;

	LOG_INF("Set Signal %s -> %d", desc->name, value);
	if (desc->source == PWR_SIG_EMUL_SRC_GPIO) {
		power_signal_emul_set_gpio_value(&desc->spec.gpio, !!value);
		return;
	}

	if (desc->source == PWR_SIG_EMUL_SRC_EXT) {
		ret = power_signal_set(desc->enum_id, value);
		zassert_true((ret == 0), "Setting %s Signal value!!",
			     desc->name);
	}

	power_signal_interrupt(desc->enum_id, value);
}

static int power_signal_emul_get_gpio_value(const struct gpio_dt_spec *spec)
{
	gpio_flags_t gpio_flags;
	int ret;

	ret = gpio_emul_flags_get(spec->port, spec->pin, &gpio_flags);
	zassert_true((ret == 0), "Getting GPIO flags!!");

	if (gpio_flags & GPIO_INPUT) {
		ret = gpio_pin_get(spec->port, spec->pin);
	} else if (gpio_flags & GPIO_OUTPUT) {
		ret = gpio_emul_output_get(spec->port, spec->pin);
	}

	return ret;
}

static int
power_signal_emul_get_value(struct power_signal_emul_signal_desc *desc)
{
	int ret;

	if (desc->source == PWR_SIG_EMUL_SRC_GPIO) {
		ret = power_signal_emul_get_gpio_value(&desc->spec.gpio);
	} else {
		ret = power_signal_get(desc->enum_id);
	}

	return ret;
}

/*
 * Signals events handlers
 */
static void emul_power_signal_gpio_interrupt(const struct device *port,
					     struct gpio_callback *cb,
					     gpio_port_pins_t pins)
{
	struct power_signal_emul_output *out_signal =
		CONTAINER_OF(cb, struct power_signal_emul_output, cb);
	struct power_signal_emul_node *node =
		CONTAINER_OF(out_signal, struct power_signal_emul_node, output);
	int out_value;
	int delay;
	int new_value;

	out_value = power_signal_emul_get_value(&out_signal->desc);
	if (out_value == out_signal->value) {
		return;
	}

	out_signal->value = out_value;
	if (out_signal->edge == EDGE_ACTIVE_ON_DEASSERT &&
	    out_value == out_signal->assert_value) {
		return;
	} else if (out_signal->edge == EDGE_ACTIVE_ON_ASSERT &&
		   out_value != out_signal->assert_value) {
		return;
	}

	LOG_INF("INT: Set Signal %s -> %d", out_signal->desc.name, out_value);
	for (int i = 0; i < node->inputs_count; i++) {
		struct power_signal_emul_input *in_signal = &node->inputs[i];

		new_value = (out_value == out_signal->assert_value) ^
					    in_signal->invert ?
				    in_signal->assert_value :
				    in_signal->deassert_value;

		delay = (out_value == out_signal->assert_value) ?
				in_signal->assert_delay_ms :
				in_signal->deassert_delay_ms;

		in_signal->value = new_value;

		LOG_INF("INT: Delay Signal %s", in_signal->desc.name);
		k_work_schedule(&node->inputs[i].d_work, K_MSEC(delay));
	}
}

static void emul_signal_work_hanlder(struct k_work *work)
{
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct power_signal_emul_input *in_signal =
		CONTAINER_OF(d_work, struct power_signal_emul_input, d_work);

	power_signal_emul_set_value(&in_signal->desc, in_signal->value);
}

/*
 * Nodes manipulation functions
 */
static void power_siganl_init_node(struct power_signal_emul_node *node)
{
	struct power_signal_emul_output *out_signal = &node->output;
	struct power_signal_emul_input *in_signal;

	if (!node->inputs_count) {
		LOG_WRN("Node does not have input signal");
		return;
	}

	LOG_INF("Initializing node: %s", node->name);
	for (int in_count = 0; in_count < node->inputs_count; in_count++) {
		in_signal = &node->inputs[in_count];

		if (in_signal->retain) {
			in_signal->value =
				power_signal_emul_get_value(&in_signal->desc);
		} else {
			/* Not retaining previous value, override */
			power_signal_emul_set_value(&in_signal->desc,
						    in_signal->init_value);
			in_signal->value = in_signal->init_value;
		}
		k_work_init_delayable(&in_signal->d_work,
				      emul_signal_work_hanlder);
	}

	if (out_signal->retain) {
		out_signal->value =
			power_signal_emul_get_value(&out_signal->desc);
	} else {
		/* Not retaining previous value, override */
		power_signal_emul_set_value(&out_signal->desc,
					    out_signal->init_value);
		out_signal->value = out_signal->init_value;
	}
	if (out_signal->desc.source == PWR_SIG_EMUL_SRC_GPIO) {
		gpio_init_callback(&out_signal->cb,
				   emul_power_signal_gpio_interrupt,
				   BIT(out_signal->desc.spec.gpio.pin));

		gpio_add_callback(out_signal->desc.spec.gpio.port,
				  &out_signal->cb);

		gpio_pin_interrupt_configure_dt(&out_signal->desc.spec.gpio,
						GPIO_INT_EDGE_BOTH);
	}
}

/*
 * Exported functions
 */
int power_signal_emul_load(enum test_platforms_id test_id)
{
	if (test_id >= TEST_ID_COUNT) {
		return -EINVAL;
	}

	if (cur_test_platform) {
		return -EBUSY;
	}

	cur_test_platform = &test_platforms[test_id];

	LOG_INF("Loading Emulator test: %s", cur_test_platform->name_id);

	for (int i = 0; i < cur_test_platform->nodes_count; i++) {
		power_siganl_init_node(cur_test_platform->nodes[i]);
	}
	LOG_INF("Loading Emulator test Done");
	return 0;
}

int power_signal_emul_unload(void)
{
	struct power_signal_emul_node *node;
	struct power_signal_emul_output *out_signal;
	struct power_signal_emul_input *in_signal;

	if (!cur_test_platform) {
		return -EINVAL;
	}

	for (int i = 0; i < cur_test_platform->nodes_count; i++) {
		node = cur_test_platform->nodes[i];
		out_signal = &node->output;

		if (out_signal->desc.source != PWR_SIG_EMUL_SRC_GPIO) {
			/* Currently, Only output GPIO signals are supported */
			continue;
		}

		for (int in_count = 0; in_count < node->inputs_count;
		     in_count++) {
			static struct k_work_sync work_sync;
			in_signal = &node->inputs[in_count];

			k_work_cancel_delayable_sync(&in_signal->d_work,
						     &work_sync);
		}
		gpio_pin_interrupt_configure_dt(&out_signal->desc.spec.gpio,
						GPIO_INT_DISABLE);
		gpio_remove_callback(out_signal->desc.spec.gpio.port,
				     &out_signal->cb);
	}
	cur_test_platform = NULL;
	return 0;
}
