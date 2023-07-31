/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cec.h"
#include "driver/cec/bitbang.h"
#include "driver/cec/it83xx.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "timer.h"

#include <zephyr/ztest.h>

#define TEST_PORT 1

#define CEC_GPIO_PORT(name) \
	DEVICE_DT_GET(DT_GPIO_CTLR(NAMED_GPIOS_GPIO_NODE(name), gpios))
#define CEC_GPIO_PIN(name) DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(name), gpios)

#define CEC_OUT_PORT CEC_GPIO_PORT(gpio_hdmi_cec_out)
#define CEC_OUT_PIN CEC_GPIO_PIN(gpio_hdmi_cec_out)
#define CEC_IN_PORT CEC_GPIO_PORT(gpio_hdmi_cec_in)
#define CEC_IN_PIN CEC_GPIO_PIN(gpio_hdmi_cec_in)
#define CEC_PULL_UP_PORT CEC_GPIO_PORT(gpio_hdmi_cec_pull_up)
#define CEC_PULL_UP_PIN CEC_GPIO_PIN(gpio_hdmi_cec_pull_up)

struct mock_it83xx_cec_regs mock_it83xx_cec_regs;

/* Timestamp when the timer was last started */
static timestamp_t start_time;

/* The capture edge we're waiting for */
static enum cec_cap_edge expected_cap_edge;

static void edge_received_f(enum cec_cap_edge edge, int line)
{
	if (edge == CEC_CAP_EDGE_NONE || edge != expected_cap_edge)
		zassert_unreachable("Unexpected edge %d, line %d", edge, line);

	cec_event_cap(TEST_PORT);
}
#define edge_received(edge) edge_received_f(edge, __LINE__)

static void timer_expired(struct k_timer *unused)
{
	cec_event_timeout(TEST_PORT);
}
K_TIMER_DEFINE(timer, timer_expired, NULL);

void cec_tmr_cap_start(int port, enum cec_cap_edge edge, int timeout)
{
	expected_cap_edge = edge;

	if (timeout > 0) {
		start_time = get_time();
		k_timer_start(&timer, K_USEC(timeout), K_NO_WAIT);
	}
}

int cec_tmr_cap_get(int port)
{
	return get_time().val - start_time.val;
}

void cec_trigger_send(int port)
{
	/* Trigger tx event directly */
	cec_event_tx(port);
}

static void cec_bitbang_after(void *fixture)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;

	/* Disable CEC after each test to reset driver state */
	drv->set_enable(TEST_PORT, 0);
}

ZTEST_USER(cec_bitbang, test_set_get_logical_addr)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	uint8_t logical_addr;

	drv->set_logical_addr(TEST_PORT, 0x4);
	drv->get_logical_addr(TEST_PORT, &logical_addr);
	zassert_equal(logical_addr, 0x4);

	drv->set_logical_addr(TEST_PORT, CEC_UNREGISTERED_ADDR);
	drv->get_logical_addr(TEST_PORT, &logical_addr);
	zassert_equal(logical_addr, CEC_UNREGISTERED_ADDR);

	drv->set_logical_addr(TEST_PORT, CEC_INVALID_ADDR);
	drv->get_logical_addr(TEST_PORT, &logical_addr);
	zassert_equal(logical_addr, CEC_INVALID_ADDR);
}

ZTEST_USER(cec_bitbang, test_set_get_enable)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	uint8_t enable;

	drv->set_enable(TEST_PORT, 1);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 1);

	drv->set_enable(TEST_PORT, 0);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 0);

	/* Enabling when enabled */
	drv->set_enable(TEST_PORT, 1);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 1);
	drv->set_enable(TEST_PORT, 1);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 1);

	/* Disabling when disabled */
	drv->set_enable(TEST_PORT, 0);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 0);
	drv->set_enable(TEST_PORT, 0);
	drv->get_enable(TEST_PORT, &enable);
	zassert_equal(enable, 0);
}

ZTEST_USER(cec_bitbang, test_send_when_disabled)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	const uint8_t msg[] = { 0x40, 0x04 };
	const uint8_t msg_len = ARRAY_SIZE(msg);
	int ret;

	/* Sending when disabled returns an error */
	drv->set_enable(TEST_PORT, 0);
	ret = drv->send(TEST_PORT, msg, msg_len);
	zassert_equal(ret, EC_ERROR_BUSY);
}

ZTEST_USER(cec_bitbang, test_send_multiple)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	const uint8_t msg[] = { 0x40, 0x04 };
	const uint8_t msg_len = ARRAY_SIZE(msg);
	int ret;

	drv->set_enable(TEST_PORT, 1);

	/* Start sending a message */
	ret = drv->send(TEST_PORT, msg, msg_len);
	zassert_equal(ret, EC_SUCCESS);
	k_sleep(K_MSEC(10));

	/* Try to send another message, check the driver returns an error */
	ret = drv->send(TEST_PORT, msg, msg_len);
	zassert_equal(ret, EC_ERROR_BUSY);
}

ZTEST_USER(cec_bitbang, test_send_success)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	const uint8_t msg[] = { 0x40, 0x04 };
	const uint8_t msg_len = ARRAY_SIZE(msg);
	struct ec_response_get_next_event_v1 event;
	int ret;

	/* Enable CEC and set logical address */
	drv->set_enable(TEST_PORT, 1);
	drv->set_logical_addr(TEST_PORT, 0x4);

	/* Start sending */
	ret = drv->send(TEST_PORT, msg, msg_len);
	zassert_equal(ret, EC_SUCCESS);

	/*
	 * Driver will automatically set timeouts and transition through the
	 * necessary states.
	 */
	k_sleep(K_SECONDS(1));

	/* TODO: Check the gpio timing */

	/* Check SEND_OK MKBP event was sent */
	zassert_ok(get_next_cec_mkbp_event(&event));
	zassert_true(cec_event_matches(&event, TEST_PORT, EC_MKBP_CEC_SEND_OK));
	zassert_not_equal(get_next_cec_mkbp_event(&event), 0);
}

ZTEST_USER(cec_bitbang, test_receive_success)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	const uint8_t msg[] = { 0x04, 0x8f };
	const uint8_t msg_len = ARRAY_SIZE(msg);
	struct ec_response_get_next_event_v1 event;
	struct ec_response_cec_read response;

	/* Enable CEC and set logical address */
	drv->set_enable(TEST_PORT, 1);
	drv->set_logical_addr(TEST_PORT, 0x4);

	/* Receive start bit */
	edge_received(CEC_CAP_EDGE_FALLING);
	k_sleep(K_USEC(CEC_START_BIT_LOW_US));
	edge_received(CEC_CAP_EDGE_RISING);
	k_sleep(K_USEC(CEC_START_BIT_HIGH_US));

	for (int i = 0; i < msg_len; i++) {
		/* Receive data bits */
		for (int j = 7; j >= 0; j--) {
			if (msg[i] & BIT(j)) {
				/* 1 bit */
				edge_received(CEC_CAP_EDGE_FALLING);
				k_sleep(K_USEC(CEC_DATA_ONE_LOW_US));
				edge_received(CEC_CAP_EDGE_RISING);
				k_sleep(K_USEC(CEC_DATA_ONE_HIGH_US));
			} else {
				/* 0 bit */
				edge_received(CEC_CAP_EDGE_FALLING);
				k_sleep(K_USEC(CEC_DATA_ZERO_LOW_US));
				edge_received(CEC_CAP_EDGE_RISING);
				k_sleep(K_USEC(CEC_DATA_ZERO_HIGH_US));
			}
		}

		if (i == msg_len - 1) {
			/* EOM is set */
			edge_received(CEC_CAP_EDGE_FALLING);
			k_sleep(K_USEC(CEC_DATA_ONE_LOW_US));
			edge_received(CEC_CAP_EDGE_RISING);
			k_sleep(K_USEC(CEC_DATA_ONE_HIGH_US));
		} else {
			/* EOM is cleared */
			edge_received(CEC_CAP_EDGE_FALLING);
			k_sleep(K_USEC(CEC_DATA_ZERO_LOW_US));
			edge_received(CEC_CAP_EDGE_RISING);
			k_sleep(K_USEC(CEC_DATA_ZERO_HIGH_US));
		}

		/*
		 * Receive ACK bit falling edge, then wait one period before the
		 * next data bit falling edge.
		 */
		edge_received(CEC_CAP_EDGE_FALLING);
		k_sleep(K_USEC(CEC_NOMINAL_BIT_PERIOD_US));

		/* TODO: check we assert the ACK bit? */
	}

	k_sleep(K_SECONDS(1));

	/*
	 * Message complete, so driver will set CEC_TASK_EVENT_RECEIVED_DATA and
	 * CEC task will send MKBP event.
	 */
	zassert_ok(get_next_cec_mkbp_event(&event));
	zassert_true(
		cec_event_matches(&event, TEST_PORT, EC_MKBP_CEC_HAVE_DATA));
	zassert_not_equal(get_next_cec_mkbp_event(&event), 0);

	/* Send read command and check response contains the correct message */
	zassert_ok(host_cmd_cec_read(TEST_PORT, &response));
	zassert_equal(response.msg_len, msg_len);
	zassert_ok(memcmp(response.msg, msg, msg_len));
}

ZTEST_USER(cec_bitbang, test_receive_unavailable)
{
	const struct cec_drv *drv = cec_config[TEST_PORT].drv;
	uint8_t *msg;
	uint8_t msg_len;
	int ret;

	/*
	 * Try to get a received message when there isn't one, check the driver
	 * returns an error.
	 */
	ret = drv->get_received_message(TEST_PORT, &msg, &msg_len);
	zassert_equal(ret, EC_ERROR_UNAVAILABLE);
}

ZTEST_SUITE(cec_bitbang, drivers_predicate_post_main, NULL, NULL,
	    cec_bitbang_after, NULL);
