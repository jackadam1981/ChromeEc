/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "fpsensor_detect.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "zephyr/kernel.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/mgmt/ec_host_cmd/backend.h>
#include <zephyr/mgmt/ec_host_cmd/ec_host_cmd.h>
#include <zephyr/ztest.h>

#define UART_MAX_REQUEST_SIZE 0x220
#define SPI_MAX_REQUEST_SIZE 0x220

#define UART_MAX_RESPONSE_SIZE 0x100
#define SPI_MAX_RESPONSE_SIZE 0x220

FAKE_VALUE_FUNC(struct ec_host_cmd_backend *, ec_host_cmd_backend_get_uart,
		const struct device *);
FAKE_VALUE_FUNC(struct ec_host_cmd_backend *, ec_host_cmd_backend_get_spi,
		struct gpio_dt_spec *);
FAKE_VALUE_FUNC(int, ec_host_cmd_init, struct ec_host_cmd_backend *);

enum ec_host_cmd_status
host_command_protocol_info(struct ec_host_cmd_handler_args *args);
int fp_transport_init(void);

static void *transport_setup(void)
{
	RESET_FAKE(ec_host_cmd_backend_get_uart);
	RESET_FAKE(ec_host_cmd_backend_get_spi);
	RESET_FAKE(ec_host_cmd_init);

	return NULL;
}

ZTEST_SUITE(transport, NULL, transport_setup, NULL, NULL, NULL);

ZTEST(transport, test_transport_type)
{
	const struct device *transport_sel_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(transport_sel), gpios));
	const struct device *div_highside_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(div_highside), gpios));
	const gpio_port_pins_t transport_sel_pin =
		DT_GPIO_PIN(DT_NODELABEL(transport_sel), gpios);
	const gpio_port_pins_t div_highside_pin =
		DT_GPIO_PIN(DT_NODELABEL(div_highside), gpios);

	/* Set the transport sel pin and make sure div_highside is at the right
	 * state
	 */
	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 0);
	zassert_equal(get_fp_transport_type(), FP_TRANSPORT_TYPE_UART,
		      "Incorrect transport type");
	zassert_equal(gpio_emul_output_get(div_highside_gpio, div_highside_pin),
		      0);

	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 1);
	zassert_equal(get_fp_transport_type(), FP_TRANSPORT_TYPE_SPI,
		      "Incorrect transport type");
	zassert_equal(gpio_emul_output_get(div_highside_gpio, div_highside_pin),
		      0);
}

ZTEST(transport, test_protocol_info)
{
	const struct device *transport_sel_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(transport_sel), gpios));
	const gpio_port_pins_t transport_sel_pin =
		DT_GPIO_PIN(DT_NODELABEL(transport_sel), gpios);
	struct ec_response_get_protocol_info protocol_info;
	struct ec_host_cmd_handler_args args;

	memset(&args, 0, sizeof(args));
	memset(&protocol_info, 0, sizeof(protocol_info));
	args.output_buf = &protocol_info;

	/* UART */
	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 0);
	host_command_protocol_info(&args);
	zassert_equal(protocol_info.flags,
		      EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED);
	zassert_equal(protocol_info.max_request_packet_size,
		      UART_MAX_REQUEST_SIZE);
	zassert_equal(protocol_info.max_response_packet_size,
		      UART_MAX_RESPONSE_SIZE);
	zassert_equal(protocol_info.protocol_versions, BIT(3));
	zassert_equal(args.output_buf_size, sizeof(protocol_info));

	/* SPI */
	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 1);
	host_command_protocol_info(&args);
	zassert_equal(protocol_info.flags,
		      EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED);
	zassert_equal(protocol_info.max_request_packet_size,
		      SPI_MAX_REQUEST_SIZE);
	zassert_equal(protocol_info.max_response_packet_size,
		      SPI_MAX_RESPONSE_SIZE);
	zassert_equal(protocol_info.protocol_versions, BIT(3));
	zassert_equal(args.output_buf_size, sizeof(protocol_info));
}

ZTEST(transport, test_hc_init)
{
	const struct device *transport_sel_gpio =
		DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(transport_sel), gpios));
	const gpio_port_pins_t transport_sel_pin =
		DT_GPIO_PIN(DT_NODELABEL(transport_sel), gpios);
	const struct device *const dev_uart =
		DEVICE_DT_GET(DT_CHOSEN(zephyr_host_cmd_uart_backend));
	struct ec_host_cmd_backend *backend;

	/* UART */
	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 0);
	backend = (struct ec_host_cmd_backend *)0xABCD;
	SET_RETURN_SEQ(ec_host_cmd_backend_get_uart, &backend, 1);
	zassert_equal(fp_transport_init(), 0);
	zassert_equal(ec_host_cmd_backend_get_uart_fake.arg0_history[0],
		      dev_uart);
	zassert_equal(ec_host_cmd_init_fake.arg0_history[0], backend);

	/* SPI */
	gpio_emul_input_set(transport_sel_gpio, transport_sel_pin, 1);
	backend = (struct ec_host_cmd_backend *)0xDCBA;
	SET_RETURN_SEQ(ec_host_cmd_backend_get_spi, &backend, 1);
	zassert_equal(fp_transport_init(), 0);
	zassert_equal(ec_host_cmd_init_fake.arg0_history[1], backend);
}
