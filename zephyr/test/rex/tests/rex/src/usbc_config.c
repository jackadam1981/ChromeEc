/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "usbc_config.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define C0_CTRL DT_GPIO_CTLR(DT_NODELABEL(gpio_usb_c0_bc12_int_l), gpios)
#define C0_PIN DT_GPIO_PIN(DT_NODELABEL(gpio_usb_c0_bc12_int_l), gpios)
#define C0_INTR GPIO_INT_FROM_NODELABEL(int_usb_c0_bc12)

#define C1_CTRL DT_GPIO_CTLR(DT_NODELABEL(gpio_usb_c1_bc12_int_l), gpios)
#define C1_PIN DT_GPIO_PIN(DT_NODELABEL(gpio_usb_c1_bc12_int_l), gpios)
#define C1_INTR GPIO_INT_FROM_NODELABEL(int_usb_c1_bc12)

FAKE_VOID_FUNC(usb_charger_task_set_event, int, uint8_t);

static void usbc_config_before(void *fixture)
{
	ARG_UNUSED(fixture);

	const struct device *c0_dev = DEVICE_DT_GET(C0_CTRL);
	const struct device *c1_dev = DEVICE_DT_GET(C1_CTRL);

	zassert_ok(gpio_emul_input_set(c0_dev, C0_PIN, 1));
	zassert_ok(gpio_emul_input_set(c1_dev, C1_PIN, 1));

	RESET_FAKE(usb_charger_task_set_event);
}

static void mock_usb_charger_task_set_event(int port, uint8_t event)
{
	if (event != USB_CHG_EVENT_BC12)
		return;

	/*
	 * Deasserting these level-interrupts using
	 * gpio_emul_input_set(DEVICE_DT_GET(C0_CTRL), C0_PIN, 1)
	 * does not work, so disable them.
	 */
	switch (port) {
	case 0:
		zassert_ok(gpio_disable_dt_interrupt(C0_INTR));
		break;
	case 1:
		zassert_ok(gpio_disable_dt_interrupt(C1_INTR));
		break;
	};
}

ZTEST_USER(usbc_config, test_trigger_bc12_interrupt)
{
	const struct device *c0_dev = DEVICE_DT_GET(C0_CTRL);
	const struct device *c1_dev = DEVICE_DT_GET(C1_CTRL);

	usb_charger_task_set_event_fake.custom_fake =
		mock_usb_charger_task_set_event;

	zassert_ok(gpio_enable_dt_interrupt(C0_INTR));
	zassert_ok(gpio_enable_dt_interrupt(C1_INTR));

	zassert_ok(gpio_emul_input_set(c0_dev, C0_PIN, 0));
	k_sleep(K_MSEC(100));
	zassert_equal(usb_charger_task_set_event_fake.call_count, 1);
	zassert_equal(usb_charger_task_set_event_fake.arg0_val, 0);
	zassert_equal(usb_charger_task_set_event_fake.arg1_val,
		      USB_CHG_EVENT_BC12);

	zassert_ok(gpio_emul_input_set(c1_dev, C1_PIN, 0));
	k_sleep(K_MSEC(100));
	zassert_equal(usb_charger_task_set_event_fake.call_count, 2);
	zassert_equal(usb_charger_task_set_event_fake.arg0_val, 1);
	zassert_equal(usb_charger_task_set_event_fake.arg1_val,
		      USB_CHG_EVENT_BC12);
}

ZTEST_SUITE(usbc_config, NULL, NULL, usbc_config_before, NULL, NULL);
