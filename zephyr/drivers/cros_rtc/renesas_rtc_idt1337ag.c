/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT renesas_rtc_idt1337ag

#include <assert.h>
#include <device.h>
#include <drivers/cros_rtc.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <kernel.h>
#include <rtc.h>
#include <soc.h>

#include "renesas_rtc_idt1337ag.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(cros_rtc, LOG_LEVEL_ERR);

/* Driver config */
struct renesas_rtc_idt1337ag_config {
	const struct device *bus;
	const uint16_t i2c_addr_flags;
	const struct gpio_dt_spec gpio_alert;
};

/* Driver data */
struct renesas_rtc_idt1337ag_data {
	const struct device *dev;
	uint8_t time_reg[NUM_TIMER_REGS];
	struct gpio_callback gpio_cb;
	cros_rtc_alarm_callback_t alarm_callback;
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) \
	((const struct renesas_rtc_idt1337ag_config *)(dev)->config)
#define DRV_DATA(dev) ((struct renesas_rtc_idt1337ag_data *)(dev)->data)

/*
 * is_alarm == true: Reads alarm registers SECONDS, MINUTES, HOURS, and DAYS
 * is_alarm == false: Reads time registers SECONDS, MINUTES, HOURS, DAYS, and
 *			MONTHS, YEARS
 */
static int idt1337ag_read_time_regs(const struct device *dev, bool is_alarm)
{
	const struct renesas_rtc_idt1337ag_config *const config =
							DRV_CONFIG(dev);
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);
	uint8_t start_reg;
	uint8_t num_reg;

	if (is_alarm) {
		start_reg = REG_SECOND_ALARM1;
		num_reg = NUM_ALARM_REGS;
	} else {
		start_reg = REG_SECONDS;
		num_reg = NUM_TIMER_REGS;
	}

	return i2c_burst_read(config->bus,
		config->i2c_addr_flags, start_reg, data->time_reg, num_reg);
}

static int idt1337ag_read_reg(const struct device *dev,
						uint8_t reg, uint8_t *val)
{
	const struct renesas_rtc_idt1337ag_config *const config
						= DRV_CONFIG(dev);

	return i2c_reg_read_byte(config->bus, config->i2c_addr_flags, reg, val);
}

/*
 * is_alarm == true: Writes alarm registers SECONDS, MINUTES, HOURS, and DAYS
 * is_alarm == false: Writes time registers SECONDS, MINUTES, HOURS, DAYS, and
 *			MONTHS, YEARS
 */
static int idt1337ag_write_time_regs(const struct device *dev, bool is_alarm)
{
	const struct renesas_rtc_idt1337ag_config *const config =
							DRV_CONFIG(dev);
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);
	uint8_t tx_buf[NUM_TIMER_REGS];
	uint8_t start_reg;
	uint8_t num_reg;

	if (is_alarm) {
		/* Select the DAY register */
		data->time_reg[DAYS] |= SELECT_DAYS_ALARM;

		start_reg = REG_SECOND_ALARM1;
		num_reg = NUM_ALARM_REGS;
	} else {
		start_reg = REG_SECONDS;
		num_reg = NUM_TIMER_REGS;
	}

	for (int i = 0; i < num_reg; i++) {
		tx_buf[i] = data->time_reg[i];
	}

	return i2c_burst_write(config->bus,
			config->i2c_addr_flags, start_reg, tx_buf, num_reg);
}

static int idt1337ag_write_reg(const struct device *dev,
						uint8_t reg, uint8_t val)
{
	const struct renesas_rtc_idt1337ag_config *const config =
							DRV_CONFIG(dev);
	uint8_t tx_buf[2];

	tx_buf[0] = reg;
	tx_buf[1] = val;

	return i2c_write(config->bus,
		tx_buf, sizeof(tx_buf), config->i2c_addr_flags);
}

/*
 * val bits 7 to 4 - tens place
 * val bits 3 to 0 - ones place
 */
static int bcd_to_dec(uint8_t val, enum bcd_mask mask)
{
	int tens = ((val & mask) >> 4) * 10;
	int ones = (val & 0xf);

	return tens + ones;
}

/*
 * val bits 7 to 4 - tens place
 * val bits 3 to 0 - ones place
 */
static uint8_t dec_to_bcd(uint32_t val, enum bcd_mask mask)
{
	int tens = val / 10;
	int ones = val - (tens * 10);

	return ((tens << 4) & mask) | ones;
}

static int renesas_rtc_idt1337ag_read_seconds(const struct device *dev,
					uint32_t *value, bool is_alarm)
{
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);
	struct calendar_date time;
	int ret;

	ret = idt1337ag_read_time_regs(dev, is_alarm);

	if (ret < 0) {
		return ret;
	}

	if (is_alarm) {
		*value = (bcd_to_dec(data->time_reg[DAYS], DAYS_MASK) *
				SECS_PER_DAY) +
			(bcd_to_dec(data->time_reg[HOURS], HOURS24_MASK) *
				SECS_PER_HOUR) +
			(bcd_to_dec(data->time_reg[MINUTES], MINUTES_MASK) *
				SECS_PER_MINUTE) +
			bcd_to_dec(data->time_reg[SECONDS], SECONDS_MASK);
	} else {
		time.year = bcd_to_dec(data->time_reg[YEARS], YEARS_MASK);
		time.month =
			bcd_to_dec(data->time_reg[MONTHS], MONTHS_MASK);
		time.day = bcd_to_dec(data->time_reg[DAYS], DAYS_MASK);

		*value = date_to_sec(time) - SECS_TILL_YEAR_2K +
			(bcd_to_dec(data->time_reg[HOURS], HOURS24_MASK) *
				SECS_PER_HOUR) +
			(bcd_to_dec(data->time_reg[MINUTES], MINUTES_MASK) *
				SECS_PER_MINUTE) +
			bcd_to_dec(data->time_reg[SECONDS], SECONDS_MASK);
	}

	return ret;
}

static int renesas_rtc_idt1337ag_write_seconds(const struct device *dev,
				uint32_t value, bool is_alarm)
{
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);
	struct calendar_date time;
	uint32_t tmp_sec;

	time = sec_to_date(value + SECS_TILL_YEAR_2K);

	if (!is_alarm) {
		data->time_reg[YEARS] = dec_to_bcd(time.year, YEARS_MASK);
		data->time_reg[MONTHS] =
				dec_to_bcd(time.month, MONTHS_MASK);
	}

	data->time_reg[DAYS] = dec_to_bcd(time.day, DAYS_MASK);

	value %= SECS_PER_DAY;
	tmp_sec = value / SECS_PER_HOUR;
	data->time_reg[HOURS] = dec_to_bcd(tmp_sec, HOURS24_MASK);

	value -= (tmp_sec * SECS_PER_HOUR);
	tmp_sec = value / SECS_PER_MINUTE;
	data->time_reg[MINUTES] = dec_to_bcd(tmp_sec, MINUTES_MASK);

	value -= (tmp_sec * SECS_PER_MINUTE);
	data->time_reg[SECONDS] = dec_to_bcd(value, SECONDS_MASK);

	return idt1337ag_write_time_regs(dev, is_alarm);
}

static int renesas_rtc_idt1337ag_configure(const struct device *dev,
				   cros_rtc_alarm_callback_t callback)
{
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);

	if (callback == NULL) {
		return -EINVAL;
	}

	data->alarm_callback = callback;

	return 0;
}

static int renesas_rtc_idt1337ag_get_value(const struct device *dev,
							uint32_t *value)
{
	return renesas_rtc_idt1337ag_read_seconds(dev, value, false);
}

static int renesas_rtc_idt1337ag_set_value(const struct device *dev,
							uint32_t value)
{
	return renesas_rtc_idt1337ag_write_seconds(dev, value, false);
}

static int renesas_rtc_idt1337ag_get_alarm(const struct device *dev,
			uint32_t *seconds, uint32_t *microseconds)
{
	*microseconds = 0;
	return renesas_rtc_idt1337ag_read_seconds(dev, seconds, true);
}

static int renesas_rtc_idt1337ag_reset_alarm(const struct device *dev)
{
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);
	int ret;
	uint8_t val;

	ret = idt1337ag_read_reg(dev, REG_CONTROL, &val);

	if (ret < 0) {
		return ret;
	}

	/* Disable alarm interrupt and clear pending alarm flag */
	val &= ~CONTROL_A1IE;
	ret = idt1337ag_write_reg(dev, REG_CONTROL, val);

	if (ret < 0) {
		return ret;
	}

	/* Clear alarm1 flag if set */
	ret = idt1337ag_read_reg(dev, REG_STATUS, &val);

	if (ret < 0) {
		return ret;
	}

	/* Clear the alarm1 and alarm2 flag */
	val &= ~(STATUS_A1F | STATUS_A2F);
	ret = idt1337ag_write_reg(dev, REG_STATUS, val);

	if (ret < 0) {
		return ret;
	}

	/* Clear and disable the alarm registers */
	data->time_reg[SECONDS] = DISABLE_ALARM;
	data->time_reg[MINUTES] = DISABLE_ALARM;
	data->time_reg[HOURS] = DISABLE_ALARM;
	data->time_reg[DAYS] = DISABLE_ALARM;

	return idt1337ag_write_time_regs(dev, true);
}

static int renesas_rtc_idt1337ag_set_alarm(const struct device *dev,
			uint32_t seconds, uint32_t microseconds)
{
	int ret;
	uint8_t val;

	ARG_UNUSED(microseconds);

	ret = renesas_rtc_idt1337ag_reset_alarm(dev);
	if (ret < 0) {
		return ret;
	}

	ret = renesas_rtc_idt1337ag_write_seconds(dev, seconds, true);
	if (ret < 0) {
		return ret;
	}

	ret = idt1337ag_read_reg(dev, REG_CONTROL, &val);
	if (ret < 0) {
		return ret;
	}

	val |= CONTROL_A1IE;
	idt1337ag_write_reg(dev, REG_CONTROL, val);

	return 0;
}

static void renesas_rtc_idt1337ag_isr(const struct device *port,
				struct gpio_callback *cb, uint32_t pin)
{
	struct renesas_rtc_idt1337ag_data *data =
		CONTAINER_OF(cb, struct renesas_rtc_idt1337ag_data, gpio_cb);
	const struct device *dev = (const struct device *)data->dev;

	ARG_UNUSED(port);
	ARG_UNUSED(pin);
	ARG_UNUSED(cb);

	LOG_DBG("%s", __func__);

	/* Call callback function */
	if (data->alarm_callback) {
		data->alarm_callback(dev);
	}
}

static const struct cros_rtc_driver_api renesas_rtc_idt1337ag_driver_api = {
	.configure = renesas_rtc_idt1337ag_configure,
	.get_value = renesas_rtc_idt1337ag_get_value,
	.set_value = renesas_rtc_idt1337ag_set_value,
	.get_alarm = renesas_rtc_idt1337ag_get_alarm,
	.set_alarm = renesas_rtc_idt1337ag_set_alarm,
	.reset_alarm = renesas_rtc_idt1337ag_reset_alarm,
};

static int renesas_rtc_idt1337ag_init(const struct device *dev)
{
	const struct renesas_rtc_idt1337ag_config *const config =
							DRV_CONFIG(dev);
	struct renesas_rtc_idt1337ag_data *data = DRV_DATA(dev);
	uint8_t val;
	int ret;

	if (!device_is_ready(config->bus)) {
		LOG_ERR("Device %s is not ready", config->bus->name);
		return -ENODEV;
	}

	/*
	 * Read Control register. For normal operation,
	 * the values should be as follows:
	 *	Bit 7 (enable oscillator) : (0) normal mode
	 *	Bit 6 (unused)            : (0)
	 *	Bit 5 (unused)            : (0)
	 *	BIT 4 (RS2)               : (0) Not used when INTCN == 1
	 *	BIT 3 (RS1)               : (0) Not used when INTCN == 1
	 *	BIT 2 (INTCN)             : (1) a match between the timekeeping
	 *	                                registers and the alarm 1
	 *	                                registers activate the INTA pin
	 *	BIT 1 (A2IE)              : (0) Alarm 2 is not used
	 *	BIT 0 (A1IE)              : (1) Enables Alarm 1
	 */
	ret = idt1337ag_read_reg(dev, REG_CONTROL, &val);

	if (ret < 0) {
		return ret;
	}

	/* Make sure the oscillator is enabled */
	if (val & CONTROL_EOSC) {
		/* IDT1337AG oscillator is disabled. Enable it */
		val &= ~CONTROL_EOSC;
	}

	/* Disable Alarm 2 */
	if (val & CONTROL_A2IE) {
		/* Alarm 2 is enabled. Disable it */
		val &= ~CONTROL_A2IE;
	}

	/* Alarm 1 assert INTA pin */
	val |= CONTROL_INTCN;
	ret = idt1337ag_write_reg(dev, REG_CONTROL, val);

	if (ret < 0) {
		return ret;
	}

	/* Date register isn't used. Set it to zero */
	ret = idt1337ag_write_reg(dev, REG_DATE, 0);
	data->time_reg[DATE] = 0;

	/* Make sure the oscillator is running */
	ret = idt1337ag_read_reg(dev, REG_STATUS, &val);

	if (ret < 0) {
		return ret;
	}

	if (val & STATUS_OSF) {
		/* IDT1337AG oscillator is not running. Clear the flag */
		val &= ~STATUS_OSF;
	}

	/* Clear Alarm 2 flag if set */
	if (val & STATUS_A2F) {
		val &= ~STATUS_A2F;
	}

	ret = idt1337ag_write_reg(dev, REG_STATUS, val);

	if (ret < 0) {
		return ret;
	}

	renesas_rtc_idt1337ag_reset_alarm(dev);

	/* Disable Alarm2 */
	idt1337ag_write_reg(dev, REG_MINUTE_ALARM2, DISABLE_ALARM);
	idt1337ag_write_reg(dev, REG_HOUR_ALARM2, DISABLE_ALARM);
	idt1337ag_write_reg(dev, REG_DAY_ALARM2, DISABLE_ALARM);

	/* Configure GPIO interrupt pin for IDT1337AG alarm pin */

	if (!device_is_ready(config->gpio_alert.port)) {
		LOG_ERR("Alert GPIO device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&config->gpio_alert, GPIO_INPUT);

	if (ret < 0) {
		LOG_ERR("Could not configure RTC alert pin");
		return ret;
	}

	gpio_init_callback(&data->gpio_cb,
			renesas_rtc_idt1337ag_isr, BIT(config->gpio_alert.pin));

	ret = gpio_add_callback(config->gpio_alert.port, &data->gpio_cb);

	if (ret < 0) {
		LOG_ERR("Could not set RTC alert pin callback");
		return ret;
	}

	data->dev = dev;

	return gpio_pin_interrupt_configure_dt(&config->gpio_alert,
					       GPIO_INT_EDGE_FALLING);
}

#define IDT1337AG_INT_GPIOS \
	DT_PHANDLE_BY_IDX(DT_NODELABEL(idt1337ag), int_gpios, 0)

/*
 * dt_flags is a uint8_t type.  However, for platform/ec
 * the GPIO flags in the devicetree are expanded past 8 bits
 * to support the INPUT/OUTPUT and PULLUP/PULLDOWN properties.
 * Cast back to a gpio_dt_flags to compile, discarding the bits
 * that are not supported by the Zephyr GPIO API.
 */
#define CROS_EC_GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx)                \
	{                                                                  \
		.port =                                                    \
		DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(node_id, prop, idx)),    \
		.pin = DT_GPIO_PIN_BY_IDX(node_id, prop, idx),             \
		.dt_flags =                                                \
		(gpio_dt_flags_t)DT_GPIO_FLAGS_BY_IDX(node_id, prop, idx), \
	}

static const struct renesas_rtc_idt1337ag_config renesas_rtc_idt1337ag_cfg_0 = {
	.bus = DEVICE_DT_GET(DT_INST_BUS(0)),
	.i2c_addr_flags = DT_INST_REG_ADDR(0),
	.gpio_alert =
		CROS_EC_GPIO_DT_SPEC_GET_BY_IDX(IDT1337AG_INT_GPIOS, gpios, 0)
};

static struct renesas_rtc_idt1337ag_data renesas_rtc_idt1337ag_data_0;

DEVICE_DT_INST_DEFINE(0, renesas_rtc_idt1337ag_init, /* pm_control_fn= */ NULL,
		      &renesas_rtc_idt1337ag_data_0,
		      &renesas_rtc_idt1337ag_cfg_0,
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &renesas_rtc_idt1337ag_driver_api);
