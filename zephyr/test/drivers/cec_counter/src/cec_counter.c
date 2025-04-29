/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cec.h"
#include "driver/cec/bitbang.h"
#include "driver/cec/it83xx.h"
#include "drivers/cec_counter.h"
#include "gpio.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "timer.h"

#include <zephyr/drivers/counter.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <drivers/cec_counter.h>

#define cec_counter_dev DEVICE_DT_GET(DT_CHOSEN(cros_ec_cec_counter))

// FAKE_VALUE_FUNC(uint32_t, counter_us_to_ticks, const struct device,
// uint64_t);
FAKE_VALUE_FUNC(int, counter_set_top_value, const struct device,
		struct counter_top_cfg);

#define CEC_GPIO_PORT(name) \
	DEVICE_DT_GET(DT_GPIO_CTLR(NAMED_GPIOS_GPIO_NODE(name), gpios))
#define CEC_GPIO_PIN(name) DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(name), gpios)
#define CEC_GPIO_SIGNAL(name) GPIO_SIGNAL(DT_NODELABEL(name))

#define CEC_OUT_PORT CEC_GPIO_PORT(gpio_hdmi_cec_out)
#define CEC_OUT_PIN CEC_GPIO_PIN(gpio_hdmi_cec_out)
#define CEC_OUT_SIGNAL CEC_GPIO_SIGNAL(gpio_hdmi_cec_out)
#define CEC_IN_PORT CEC_GPIO_PORT(gpio_hdmi_cec_in)
#define CEC_IN_PIN CEC_GPIO_PIN(gpio_hdmi_cec_in)
#define CEC_IN_SIGNAL CEC_GPIO_SIGNAL(gpio_hdmi_cec_in)
#define CEC_PULL_UP_PORT CEC_GPIO_PORT(gpio_hdmi_cec_pull_up)
#define CEC_PULL_UP_PIN CEC_GPIO_PIN(gpio_hdmi_cec_pull_up)
#define CEC_PULL_UP_SIGNAL CEC_GPIO_SIGNAL(gpio_hdmi_cec_pull_up)

#define TEST_PORT 1

#define CEC_STATE_INITIATOR_ACK_LOW 13
#define CEC_STATE_FOLLOWER_ACK_LOW 25

struct mock_it83xx_cec_regs mock_it83xx_cec_regs;

/* Timestamp when the most recent interrupt occurred */
extern timestamp_t interrupt_time;

/* Timestamp when the second most recent interrupt occurred */
extern timestamp_t prev_interrupt_time;

/* Flag set when a transfer is initiated from the AP */
extern bool transfer_initiated;

/* Timestamp when the timer was last started */
int64_t start_time;

/* The capture edge we're waiting for */
static enum cec_cap_edge expected_cap_edge;

static void cec_counter_setup(void *fixture)
{
}

static void cec_counter_before(void *fixture)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;

	//	RESET_FAKE(counter_us_to_ticks);
	RESET_FAKE(counter_set_top_value);

	/* Disable CEC between each test to reset driver state */
	drv->set_enable(TEST_PORT, 0);

	/* Reset globals */
	start_time = 0;
	expected_cap_edge = CEC_CAP_EDGE_NONE;
	transfer_initiated = false;
	prev_interrupt_time.val = 0;
	interrupt_time.val = 0;
}

ZTEST_USER(cec_counter, test_cec_tmr_cap_start)
{
	int port = TEST_PORT;
	enum cec_cap_edge cap_edge = CEC_CAP_EDGE_FALLING;
	int timeout = 2 * CEC_NOMINAL_BIT_PERIOD_US;
	int delay;
	timestamp_t fake_time;
	//	uint32_t ticks;

	zassert_equal(prev_interrupt_time.val, 0);
	zassert_equal(interrupt_time.val, 0);

	fake_time.val = 0;
	get_time_mock = &fake_time;
	delay = (get_time().val - interrupt_time.val + 100);
	cros_cec_bitbang_tmr_cap_start(port, cap_edge, timeout);
	// zassert_equal(counter_us_to_ticks_fake.call_count, 1);
	zassert_equal(counter_set_top_value_fake.call_count, 1);
}

ZTEST_USER(cec_counter, test_cec_gpio_interrupt)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	const struct bitbang_cec_config *drv_config =
		cec_config[TEST_PORT].drv_config;
	uint8_t enable;
	int flags;

	drv->set_enable(TEST_PORT, 1);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 1);
	/* gpio interrupt is enabled when the bitbang cec is enable */
	gpio_pin_get_config_dt(gpio_get_dt_spec(drv_config->gpio_in), &flags);
	zassert_equal(flags, GPIO_INPUT | GPIO_INT_ENABLE | GPIO_INT_EDGE_BOTH,
		      "actual GPIO flags were %#x", flags);

	drv->set_enable(TEST_PORT, 0);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 0);
	/* gpio interrupt is disabled when the bitbang cec is disable */
	gpio_pin_get_config_dt(gpio_get_dt_spec(drv_config->gpio_in), &flags);
	zassert_equal(flags, GPIO_INPUT | GPIO_INT_DISABLE,
		      "actual GPIO flags were %#x", flags);
}

ZTEST_SUITE(cec_counter, drivers_predicate_post_main, cec_counter_setup,
	    cec_counter_before, NULL, NULL);
