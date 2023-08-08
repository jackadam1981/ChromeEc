/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define DT_DRV_COMPAT cros_gpio_controller_mock

struct gpio_controller_data {
	bool init_called;
	int pin_configure_calls;
};

int gpio_mock_controller_pin_configure_call_count(const struct device *port)
{
	struct gpio_controller_data *data = port->data;

	if (!data->init_called) {
		return -EINVAL;
	}

	return data->pin_configure_calls;
}

static int gpio_mock_controller_pin_configure(const struct device *port,
					      gpio_pin_t pin,
					      gpio_flags_t flags)
{
	struct gpio_controller_data *data = port->data;

	data->pin_configure_calls++;
	return -ENOTSUP;
}

#ifdef CONFIG_GPIO_GET_CONFIG
static int gpio_mock_controller_pin_get_config(const struct device *port,
					       gpio_pin_t pin,
					       gpio_flags_t *out_flags)
{
	return -ENOTSUP;
}
#endif

static int gpio_mock_controller_port_get_raw(const struct device *port,
					     gpio_port_value_t *values)
{
	return -ENOTSUP;
}

static int gpio_mock_controller_port_set_masked_raw(const struct device *port,
						    gpio_port_pins_t mask,
						    gpio_port_value_t values)
{
	return -ENOTSUP;
}

static int gpio_mock_controller_port_set_bits_raw(const struct device *port,
						  gpio_port_pins_t pins)
{
	return -ENOTSUP;
}

static int gpio_mock_controller_port_clear_bits_raw(const struct device *port,
						    gpio_port_pins_t pins)
{
	return -ENOTSUP;
}

static int gpio_mock_controller_port_toggle_bits(const struct device *port,
						 gpio_port_pins_t pins)
{
	return -ENOTSUP;
}

static int gpio_mock_controller_pin_interrupt_configure(
	const struct device *port, gpio_pin_t pin, enum gpio_int_mode mode,
	enum gpio_int_trig trig)
{
	return -ENOTSUP;
}

static int gpio_mock_controller_manage_callback(const struct device *port,
						struct gpio_callback *cb,
						bool set)
{
	return -ENOTSUP;
}

static gpio_port_pins_t
gpio_mock_controller_get_pending_int(const struct device *dev)
{
	return -ENOTSUP;
}

#ifdef CONFIG_GPIO_GET_DIRECTION
static int gpio_mock_controller_port_get_direction(const struct device *port,
						   gpio_port_pins_t map,
						   gpio_port_pins_t *inputs,
						   gpio_port_pins_t *outputs)
{
	return -ENOTSUP;
}
#endif

static int gpio_mock_controller_init(const struct device *dev)
{
	struct gpio_controller_data *data = dev->data;

	data->init_called = true;

	/* We always want this device to return not ready */
	return -ENOTSUP;
}

static const struct gpio_driver_api gpio_mock_controller_driver = {
	.pin_configure = gpio_mock_controller_pin_configure,
#ifdef CONFIG_GPIO_GET_CONFIG
	.pin_get_config = gpio_mock_controller_pin_get_config,
#endif
	.port_get_raw = gpio_mock_controller_port_get_raw,
	.port_set_masked_raw = gpio_mock_controller_port_set_masked_raw,
	.port_set_bits_raw = gpio_mock_controller_port_set_bits_raw,
	.port_clear_bits_raw = gpio_mock_controller_port_clear_bits_raw,
	.port_toggle_bits = gpio_mock_controller_port_toggle_bits,
	.pin_interrupt_configure = gpio_mock_controller_pin_interrupt_configure,
	.manage_callback = gpio_mock_controller_manage_callback,
	.get_pending_int = gpio_mock_controller_get_pending_int,
#ifdef CONFIG_GPIO_GET_DIRECTION
	.port_get_direction = gpio_mock_controller_port_get_direction,
#endif /* CONFIG_GPIO_GET_DIRECTION */
};

#define DEFINE_GPIO_CONTROLLER_MOCK(inst)                                     \
	static struct gpio_controller_data gpio_controller_data_##inst;       \
	DEVICE_DT_INST_DEFINE(inst, gpio_mock_controller_init, NULL /* pm */, \
			      &gpio_controller_data_##inst /* data */,        \
			      NULL /* cfg */, POST_KERNEL,                    \
			      CONFIG_GPIO_INIT_PRIORITY,                      \
			      &gpio_mock_controller_driver);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GPIO_CONTROLLER_MOCK)
