/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "timer.h"

#include <zephyr/drivers/gpio.h>

#include <ap_power/ap_power.h>

#define INT_RECHECK_US 5000

static void board_backlight_handler(struct ap_power_ev_callback *cb,
				    struct ap_power_ev_data data)
{
	switch (data.event) {
	default:
		return;

	case AP_POWER_STARTUP:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr),
				1);
		break;

	case AP_POWER_HARD_OFF:
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_en_ppvar_blpwr),
				0);
		break;
	}
}

static int install_backlight_handler(void)
{
	static struct ap_power_ev_callback cb;
	/*
	 * Add a callback for start/hardoff to
	 * control the backlight load switch.
	 */
	ap_power_ev_init_callback(&cb, board_backlight_handler,
				  AP_POWER_STARTUP | AP_POWER_HARD_OFF);
	ap_power_ev_add_callback(&cb);

	return 0;
}

SYS_INIT(install_backlight_handler, APPLICATION, 1);

static void check_audio_jack(void)
{
	if (chipset_in_or_transitioning_to_state(CHIPSET_STATE_ON)) {
		if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_jd1)))
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_5p0va_pwr_mode), 0);
		else
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_5p0va_pwr_mode), 1);
	} else {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_5p0va_pwr_mode), 0);
	}
}
DECLARE_DEFERRED(check_audio_jack);

DECLARE_HOOK(HOOK_INIT, check_audio_jack, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, check_audio_jack, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, check_audio_jack, HOOK_PRIO_DEFAULT);

void audio_jack_interrupt(enum gpio_signal s)
{
	hook_call_deferred(&check_audio_jack_data, INT_RECHECK_US);
}

#include <soc_common.h>
#include <zephyr/pm/policy.h>
#include <zephyr/drivers/spi.h>

#define GOOGLE_BYTE_PER_FRAME      1
#define HUAQIN_BYTE_PER_FRAME      5
#define OUTPUT_PATTERN_SIZE 8

static int google_spi_mosi_test(void)
{
	struct device *spi_device = (struct device *)DEVICE_DT_GET(DT_NODELABEL(spi0));
	struct spi_config spi_cfg = {
		.operation = SPI_WORD_SET(8),
		.frequency = MHZ(6),
		.slave = 0,
	};

	/* SCLK = 6MHz,
	 * one frame:  (H) 5 * (1 / 6M) = 833us (L) 3 * (1 / 6M) = 500us
	 * zero frame: (H) 2 * (1 / 6M) = 333us (L) 6 * (1 / 6M) = 1us
	 */
	uint8_t one_frame[GOOGLE_BYTE_PER_FRAME] = {0xF8};
	uint8_t zero_frame[GOOGLE_BYTE_PER_FRAME] = {0xC0};
	uint8_t output_frames[OUTPUT_PATTERN_SIZE * GOOGLE_BYTE_PER_FRAME];
	uint8_t output_seq[OUTPUT_PATTERN_SIZE] = {1, 0, 1, 0, 0, 1, 0, 1};
	int ret;

	if (!spi_device) {
		printk("null spi device\n");
		return -ENODEV;
	}

	for (uint8_t i = 0; i < sizeof(output_seq); i++) {
		memcpy(output_frames + i * GOOGLE_BYTE_PER_FRAME, output_seq[i] ? one_frame : zero_frame,
		       GOOGLE_BYTE_PER_FRAME);
	}

	const struct spi_buf frame_buffers = {.buf = output_frames, .len = sizeof(output_frames)};

	const struct spi_buf_set buf_set = {.buffers = &frame_buffers, .count = 1};

	ret = spi_write(spi_device, &spi_cfg, &buf_set);
	if (ret < 0) {
		printk("failed to send spi data %d\n", ret);
		return ret;
	}

	return 0;
}

static int huaqin_spi_mosi_test(void)
{
	struct device *spi_device = (struct device *)DEVICE_DT_GET(DT_NODELABEL(spi0));
	struct spi_config spi_cfg = {
		.operation = SPI_WORD_SET(8),
		.frequency = MHZ(48),
		.slave = 0,
	};

	/* SCLK = 48MHz,
	 * one frame:  (H) 27 * (1 / 48M) = 562us   (L) 13 * (1 / 48M) = 270.8us
	 * zero frame: (H) 13 * (1 / 48M) = 270.8us (L) 27 * (1 / 48M) = 562us
	 */
	uint8_t one_frame[HUAQIN_BYTE_PER_FRAME] = {0xFF, 0xFF, 0xFF, 0xE0, 0x00};
	uint8_t zero_frame[HUAQIN_BYTE_PER_FRAME] = {0xFF, 0xF8, 0x00, 0x00, 0x00};
	uint8_t output_frames[OUTPUT_PATTERN_SIZE * HUAQIN_BYTE_PER_FRAME];
	uint8_t output_seq[OUTPUT_PATTERN_SIZE] = {1, 0, 1, 0, 0, 1, 0, 1};
	int ret;

	if (!spi_device) {
		printk("null spi device\n");
		return -ENODEV;
	}

	for (uint8_t i = 0; i < sizeof(output_seq); i++) {
		memcpy(output_frames + i * HUAQIN_BYTE_PER_FRAME, output_seq[i] ? one_frame : zero_frame,
		       HUAQIN_BYTE_PER_FRAME);
	}

	const struct spi_buf frame_buffers = {.buf = output_frames, .len = sizeof(output_frames)};

	const struct spi_buf_set buf_set = {.buffers = &frame_buffers, .count = 1};

	ret = spi_write(spi_device, &spi_cfg, &buf_set);
	if (ret < 0) {
		printk("failed to send spi data %d\n", ret);
		return ret;
	}

	return 0;
}

static void board_setup_init()
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_jd1));

	/* Block to enter power policy and idle mode. */
	chip_block_idle();
	pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);

	google_spi_mosi_test();
	huaqin_spi_mosi_test();
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);
