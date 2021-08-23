/*
 * Copyright 2021 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_rtc_pcf85063a

#include <assert.h>
#include <device.h>
#include <drivers/cros_rtc.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <kernel.h>
#include <soc.h>

#include "ec_tasks.h"
#include "nxp_rtc_pcf85063a.h"
#include "rtc.h"
#include "task.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(cros_rtc, LOG_LEVEL_ERR);

#define PCF85063A_REG_NUM       18
#define SOFT_RESET              0x58
#define CONTROL_1_DEFAULT_VALUE 0
#define OS_BIT                  0x80
#define DISABLE_ALARM           0x80

#define NUM_TIMER_REGS       7
#define NUM_ALARM_REGS       4

#define REG_CONTROL_1        0x00
#define REG_CONTROL_2        0x01
#define REG_OFFSET           0x02
#define REG_RAM_BYTE         0x03
#define REG_SECONDS          0x04
#define REG_MINUTES          0x05
#define REG_HOURS            0x06
#define REG_DAYS             0x07
#define REG_WEEKDAYS         0x08
#define REG_MONTHS           0x09
#define REG_YEARS            0x0a
#define REG_SECOND_ALARM     0x0b
#define REG_MINUTE_ALARM     0x0c
#define REG_HOUR_ALARM       0x0d
#define REG_DAY_ALARM        0x0e
#define REG_WEEKDAY_ALARM    0x0f
#define REG_TIMER_VALUE      0x10
#define REG_TIMER_MODE       0x11

/* Macros for indexing rtc_time_reg buffer */
#define SECONDS           0
#define MINUTES           1
#define HOURS             2
#define DAYS              3
#define WEEKDAYS          4
#define MONTHS            5
#define YEARS             6

enum bcd_mask {
	SECONDS_MASK = 0x70,
	MINUTES_MASK = 0x70,
	HOURS24_MASK = 0x30,
	DAYS_MASK    = 0x30,
	MONTHS_MASK  = 0x10,
	YEARS_MASK   = 0xf0
};

/* Driver config */
struct nxp_rtc_pcf85063a_config {
	const struct device *bus;
	const uint16_t i2c_slv_addr;
	const struct gpio_dt_spec gpio_alert;
};

/* Driver data */
struct nxp_rtc_pcf85063a_data {
	const struct device *dev;
	uint8_t rtc_time_reg[NUM_TIMER_REGS];
	struct gpio_callback gpio_cb;
	cros_rtc_alarm_callback_t alarm_callback;
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct nxp_rtc_pcf85063a_config *)(dev)->config)
#define DRV_DATA(dev) ((struct nxp_rtc_pcf85063a_data *)(dev)->data)

/*
 * is_alarm == true: Reads alarm registers SECONDS, MINUTES, HOURS, and DAYS
 * is_alarm == false: Reads time registers SECONDS, MINUTES, HOURS, DAYS, and
 *			MONTHS, YEARS
 */
static int pcf85063a_read_time_regs(const struct device *dev, bool is_alarm)
{
	const struct nxp_rtc_pcf85063a_config *const config = DRV_CONFIG(dev);
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);
	uint8_t start_reg;
	uint8_t num_reg;

	if (is_alarm) {
		start_reg = REG_SECOND_ALARM;
		num_reg = NUM_ALARM_REGS;
	} else {
		start_reg = REG_SECONDS;
		num_reg = NUM_TIMER_REGS;
	}

	return i2c_burst_read(config->bus,
		config->i2c_slv_addr, start_reg, data->rtc_time_reg, num_reg);
}

static int pcf85063a_read_reg(const struct device *dev,
						uint8_t reg, uint8_t *val)
{
	const struct nxp_rtc_pcf85063a_config *const config = DRV_CONFIG(dev);

	return i2c_reg_read_byte(config->bus, config->i2c_slv_addr, reg, val);
}

/*
 * is_alarm == true: Writes alarm registers SECONDS, MINUTES, HOURS, and DAYS
 * is_alarm == false: Writes time registers SECONDS, MINUTES, HOURS, DAYS, and
 *			MONTHS, YEARS
 */
static int pcf85063a_write_time_regs(const struct device *dev, bool is_alarm)
{
	const struct nxp_rtc_pcf85063a_config *const config = DRV_CONFIG(dev);
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);
	uint8_t tx_buf[NUM_TIMER_REGS];
	uint8_t start_reg;
	uint8_t num_reg;

	if (is_alarm) {
		start_reg = REG_SECOND_ALARM;
		num_reg = NUM_ALARM_REGS;
	} else {
		start_reg = REG_SECONDS;
		num_reg = NUM_TIMER_REGS;
	}

	for (int i = 0; i < num_reg; i++) {
		tx_buf[i] = data->rtc_time_reg[i];
	}

	return i2c_burst_write(config->bus,
			config->i2c_slv_addr, start_reg, tx_buf, num_reg);
}


static int pcf85063a_write_reg(const struct device *dev,
						uint8_t reg, uint8_t val)
{
	const struct nxp_rtc_pcf85063a_config *const config = DRV_CONFIG(dev);
	uint8_t tx_buf[2];

	tx_buf[0] = reg;
	tx_buf[1] = val;

	return i2c_write(config->bus,
		tx_buf, sizeof(tx_buf), config->i2c_slv_addr);
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

static int nxp_rtc_pcf85063a_read_seconds(const struct device *dev,
					uint32_t *value, bool is_alarm)
{
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);
	struct calendar_date time;
	int ret;

	ret = pcf85063a_read_time_regs(dev, is_alarm);

	if (ret < 0) {
		return ret;
	}

	if (is_alarm) {
		*value = (bcd_to_dec(data->rtc_time_reg[DAYS], DAYS_MASK) *
				SECS_PER_DAY) +
			(bcd_to_dec(data->rtc_time_reg[HOURS], HOURS24_MASK) *
				SECS_PER_HOUR) +
			(bcd_to_dec(data->rtc_time_reg[MINUTES], MINUTES_MASK) *
				SECS_PER_MINUTE) +
			bcd_to_dec(data->rtc_time_reg[SECONDS], SECONDS_MASK);
	} else {
		time.year = bcd_to_dec(data->rtc_time_reg[YEARS], YEARS_MASK);
		time.month =
			bcd_to_dec(data->rtc_time_reg[MONTHS], MONTHS_MASK);
		time.day = bcd_to_dec(data->rtc_time_reg[DAYS], DAYS_MASK);

		*value = date_to_sec(time) - SECS_TILL_YEAR_2K +
			(bcd_to_dec(data->rtc_time_reg[HOURS], HOURS24_MASK) *
				SECS_PER_HOUR) +
			(bcd_to_dec(data->rtc_time_reg[MINUTES], MINUTES_MASK) *
				SECS_PER_MINUTE) +
			bcd_to_dec(data->rtc_time_reg[SECONDS], SECONDS_MASK);
	}

	return ret;
}

static int nxp_rtc_pcf85063a_write_seconds(const struct device *dev,
				uint32_t value, bool is_alarm)
{
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);
	struct calendar_date time;
	uint32_t tmp_sec;

	time = sec_to_date(value);

	if (!is_alarm) {
		data->rtc_time_reg[YEARS] = dec_to_bcd(time.year, YEARS_MASK);
		data->rtc_time_reg[MONTHS] =
				dec_to_bcd(time.month, MONTHS_MASK);
	}

	data->rtc_time_reg[DAYS] = dec_to_bcd(time.day, DAYS_MASK);

	if (is_alarm && data->rtc_time_reg[DAYS] == 0) {
		data->rtc_time_reg[DAYS] |= DISABLE_ALARM;
	}

	value %= SECS_PER_DAY;
	tmp_sec = value / SECS_PER_HOUR;
	data->rtc_time_reg[HOURS] = dec_to_bcd(tmp_sec, HOURS24_MASK);

	if (is_alarm && data->rtc_time_reg[HOURS] == 0) {
		data->rtc_time_reg[HOURS] |= DISABLE_ALARM;
	}

	value -= (tmp_sec * SECS_PER_HOUR);
	tmp_sec = value / SECS_PER_MINUTE;
	data->rtc_time_reg[MINUTES] = dec_to_bcd(tmp_sec, MINUTES_MASK);

	if (is_alarm && data->rtc_time_reg[MINUTES] == 0) {
		data->rtc_time_reg[MINUTES] |= DISABLE_ALARM;
	}

	value -= (tmp_sec * SECS_PER_MINUTE);
	data->rtc_time_reg[SECONDS] = dec_to_bcd(value, SECONDS_MASK);

	if (is_alarm && data->rtc_time_reg[SECONDS] == 0) {
		data->rtc_time_reg[SECONDS] |= DISABLE_ALARM;
	}

	return pcf85063a_write_time_regs(dev, is_alarm);
}

static int nxp_rtc_pcf85063a_configure(const struct device *dev,
				   cros_rtc_alarm_callback_t callback)
{
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);

	if (callback == NULL) {
		return -EINVAL;
	}

	data->alarm_callback = callback;

	return 0;
}

static int nxp_rtc_pcf85063a_get_value(const struct device *dev,
							uint32_t *value)
{
	return nxp_rtc_pcf85063a_read_seconds(dev, value, false);
}

static int nxp_rtc_pcf85063a_set_value(const struct device *dev, uint32_t value)
{
	return nxp_rtc_pcf85063a_write_seconds(dev, value, false);
}

static int nxp_rtc_pcf85063a_get_alarm(const struct device *dev,
			uint32_t *seconds, uint32_t *microseconds)
{
	*microseconds = 0;
	return nxp_rtc_pcf85063a_read_seconds(dev, seconds, true);
}

static int nxp_rtc_pcf85063a_reset_alarm(const struct device *dev)
{
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);
	int ret;

	/* Disable alarm interrupt and clear pending alarm flag */
	ret = pcf85063a_write_reg(dev, REG_CONTROL_2, 0);
	if (ret < 0) {
		return ret;
	}

	/* Clear and disable the alarm registers */
	data->rtc_time_reg[SECONDS] = DISABLE_ALARM;
	data->rtc_time_reg[MINUTES] = DISABLE_ALARM;
	data->rtc_time_reg[HOURS] = DISABLE_ALARM;
	data->rtc_time_reg[DAYS] = DISABLE_ALARM;

	return pcf85063a_write_time_regs(dev, true);
}

static int nxp_rtc_pcf85063a_set_alarm(const struct device *dev,
			uint32_t seconds, uint32_t microseconds)
{
	int ret;

	ARG_UNUSED(microseconds);

	ret = nxp_rtc_pcf85063a_reset_alarm(dev);

	if (ret < 0) {
		return ret;
	}

	ret = nxp_rtc_pcf85063a_write_seconds(dev, seconds, true);

	if (ret < 0) {
		return ret;
	}

	return pcf85063a_write_reg(dev, REG_CONTROL_2, 0x80);
}

static void nxp_pcf85063a_isr(const struct device *port,
				struct gpio_callback *cb, uint32_t pin)
{
	struct nxp_rtc_pcf85063a_data *data =
		CONTAINER_OF(cb, struct nxp_rtc_pcf85063a_data, gpio_cb);
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

static const struct cros_rtc_driver_api nxp_rtc_pcf85063a_driver_api = {
	.configure = nxp_rtc_pcf85063a_configure,
	.get_value = nxp_rtc_pcf85063a_get_value,
	.set_value = nxp_rtc_pcf85063a_set_value,
	.get_alarm = nxp_rtc_pcf85063a_get_alarm,
	.set_alarm = nxp_rtc_pcf85063a_set_alarm,
	.reset_alarm = nxp_rtc_pcf85063a_reset_alarm,
};

static int nxp_rtc_pcf85063a_init(const struct device *dev)
{
	const struct nxp_rtc_pcf85063a_config *const config = DRV_CONFIG(dev);
	struct nxp_rtc_pcf85063a_data *data = DRV_DATA(dev);
	uint8_t val;
	int ret;

	if (!device_is_ready(config->bus)) {
		LOG_ERR("Device %s is not ready", config->bus->name);
		return -ENODEV;
	}

	/*
	 * Read Control_1 register. For normal operation,
	 * the values should be as follows:
	 *	Bit 7 (external clock test mode)    : (0) normal mode
	 *	Bit 6 (unused)                      : (0)
	 *	Bit 5 (STOP bit)                    : (0) RTC clock runs
	 *	BIT 4 (software reset)              : (0) no software reset
	 *	BIT 3 (unused)                      : (0)
	 *	BIT 2 (correction interrupt enable) : (0) no correction
	 *	                                          interrupt generated
	 *	BIT 1 (12 or 24-hour mode)          : (0) 24-hour mode
	 *	BIT 0 (internal oscillator capacitor: (0) 7pF
	 */
	ret = pcf85063a_read_reg(dev, REG_CONTROL_1, &val);

	if (ret < 0) {
		return ret;
	}

	if (val != CONTROL_1_DEFAULT_VALUE) {
		/* PCF85063A is not initialized, so send soft reset */
		ret = pcf85063a_write_reg(dev, REG_CONTROL_1, SOFT_RESET);

		if (ret < 0) {
			return ret;
		}
	}

	/*
	 * Read Seconds register and check if oscillator is stopped.
	 * If so, clear the bit.
	 */
	ret = pcf85063a_read_reg(dev, REG_SECONDS, &val);

	if (ret < 0) {
		return ret;
	}

	if (val & OS_BIT) {
		/* Oscillator stop bit is set, clear it. */
		val &= ~OS_BIT;
		ret = pcf85063a_write_reg(dev, REG_SECONDS, val);

		if (ret < 0) {
			return ret;
		}
	}

	nxp_rtc_pcf85063a_reset_alarm(dev);

	/* Configure GPIO interrupt pin for PCf85063A alarm pin */

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
			nxp_pcf85063a_isr, BIT(config->gpio_alert.pin));

	ret = gpio_add_callback(config->gpio_alert.port, &data->gpio_cb);

	if (ret < 0) {
		LOG_ERR("Could not set RTC alert pin callback");
		return ret;
	}

	data->dev = dev;

	return gpio_pin_interrupt_configure_dt(&config->gpio_alert,
					       GPIO_INT_EDGE_FALLING);
}

static const struct nxp_rtc_pcf85063a_config nxp_rtc_pcf85063a_cfg_0 = {
	.bus = DEVICE_DT_GET(DT_INST_BUS(0)),
	.i2c_slv_addr = DT_INST_REG_ADDR(0),
	.gpio_alert =
		GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(pcf85063a), irq_gpios, 0)
};

static struct nxp_rtc_pcf85063a_data nxp_rtc_pcf85063a_data_0;

DEVICE_DT_INST_DEFINE(0, nxp_rtc_pcf85063a_init, /* pm_control_fn= */ NULL,
		      &nxp_rtc_pcf85063a_data_0, &nxp_rtc_pcf85063a_cfg_0,
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &nxp_rtc_pcf85063a_driver_api);
