/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/spi_emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <emul/emul_egis630.h>

#define DT_DRV_COMPAT egis_egis630

LOG_MODULE_REGISTER(emul_egis630, LOG_LEVEL_INF);

struct egis630_emul_data {
	uint8_t low_power_mode;
	struct gpio_callback irq_cb;
	const struct emul *target;
	bool stop_irq;
	bool stop_spi;
};

struct egis630_emul_cfg {
	struct gpio_dt_spec interrupt_pin;
	struct gpio_dt_spec reset_pin;
};

enum egis630_cmd {
	EGIS630_CMD_INT_CLR = 0x1C,
	EGIS630_CMD_HW_ID = 0xFD,
};

uint8_t egis630_get_low_power_mode(const struct emul *target)
{
	struct egis630_emul_data *data = target->data;

	return data->low_power_mode;
}

void egis630_stop_irq(const struct emul *target)
{
	struct egis630_emul_data *data = target->data;

	data->stop_irq = true;
}

void egis630_stop_spi(const struct emul *target)
{
	struct egis630_emul_data *data = target->data;

	data->stop_spi = true;
}

void egis630_start_spi(const struct emul *target)
{
	struct egis630_emul_data *data = target->data;

	data->stop_spi = false;
}

static void egis630_write_response(const struct spi_buf_set *rx_bufs,
				   const uint8_t *resp)
{
	__ASSERT_NO_MSG(rx_bufs != NULL);
	__ASSERT_NO_MSG(rx_bufs->count == 2);
	__ASSERT_NO_MSG(resp != NULL);

	const struct spi_buf *target_rx_buf = &rx_bufs->buffers[1];
	__ASSERT_NO_MSG(target_rx_buf->buf != NULL);
	__ASSERT_NO_MSG(target_rx_buf->len > 0);

	memcpy(target_rx_buf->buf, resp, target_rx_buf->len);
}

static int egis630_emul_io(const struct emul *target,
			   const struct spi_config *config,
			   const struct spi_buf_set *tx_bufs,
			   const struct spi_buf_set *rx_bufs)
{
	const struct egis630_emul_cfg *cfg = target->cfg;
	struct egis630_emul_data *data = target->data;
	uint8_t cmd;

	if (data->stop_spi) {
		return -EINVAL;
	}

	ARG_UNUSED(config);

	__ASSERT_NO_MSG(tx_bufs != NULL);
	__ASSERT_NO_MSG(tx_bufs->buffers != NULL);
	__ASSERT_NO_MSG(tx_bufs->count > 0);
	__ASSERT_NO_MSG(tx_bufs->buffers[0].len > 0);

	/* The first byte contains the command. */
	cmd = *(uint8_t *)tx_bufs->buffers[0].buf;

	switch (cmd) {
	case EGIS630_CMD_HW_ID:
		__ASSERT_NO_MSG(rx_bufs != NULL);
		/*
		 * The first must be 0x00, because it's received when MCU is
		 * transmitting the command.
		 */
		const uint8_t resp[3] = { 0x1, 0x1E, 0x6 };

		egis630_write_response(rx_bufs, resp);
		break;

	case EGIS630_CMD_INT_CLR:
		gpio_emul_input_set(cfg->interrupt_pin.port,
				    cfg->interrupt_pin.pin, 0);
		break;

	default:
		LOG_WRN("Unimplemented command 0x%x", cmd);
	}

	return 0;
}

static struct spi_emul_api egis630_emul_api = {
	.io = egis630_emul_io,
};

static void egis630_emul_reset(const struct emul *target)
{
	struct egis630_emul_data *data = target->data;

	data->low_power_mode = 0;
	data->stop_irq = false;
	data->stop_spi = false;
}

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

/* Add test reset handlers in when using emulators with tests */
#define EGIS630_EMUL_RESET_RULE_AFTER(inst) \
	egis630_emul_reset(EMUL_DT_GET(DT_DRV_INST(inst)))

static void egis630_emul_reset_rule_after(const struct ztest_unit_test *test,
					  void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	DT_INST_FOREACH_STATUS_OKAY(EGIS630_EMUL_RESET_RULE_AFTER);
}
ZTEST_RULE(egis630_emul_reset, NULL, egis630_emul_reset_rule_after);

#endif /* CONFIG_ZTEST */

static void egis630_emul_gpio_irq(const struct device *dev,
				  struct gpio_callback *cb, uint32_t pins)
{
	struct egis630_emul_data *data =
		CONTAINER_OF(cb, struct egis630_emul_data, irq_cb);
	const struct egis630_emul_cfg *cfg = data->target->cfg;

	if (!data->stop_irq) {
		gpio_emul_input_set(cfg->interrupt_pin.port,
				    cfg->interrupt_pin.pin, 1);
	}
}

static int egis630_emul_init(const struct emul *target,
			     const struct device *parent)
{
	const struct egis630_emul_cfg *cfg = target->cfg;
	struct egis630_emul_data *data = target->data;
	ARG_UNUSED(parent);

	data->target = target;

	egis630_emul_reset(target);
	gpio_init_callback(&data->irq_cb, egis630_emul_gpio_irq,
			   BIT(cfg->reset_pin.pin));
	gpio_add_callback_dt(&cfg->reset_pin, &data->irq_cb);
	gpio_pin_interrupt_configure_dt(&cfg->reset_pin,
					GPIO_INT_EDGE_TO_INACTIVE);

	return 0;
}

#define EGIS630_EMUL(n)                                                  \
	static const struct egis630_emul_cfg egis630_emul_cfg_##n = {    \
		.interrupt_pin = GPIO_DT_SPEC_INST_GET(n, irq_gpios),    \
		.reset_pin = GPIO_DT_SPEC_INST_GET(n, reset_gpios),      \
	};                                                               \
	static struct egis630_emul_data egis630_emul_data##n;            \
	EMUL_DT_INST_DEFINE(n, egis630_emul_init, &egis630_emul_data##n, \
			    &egis630_emul_cfg_##n, &egis630_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(EGIS630_EMUL);
