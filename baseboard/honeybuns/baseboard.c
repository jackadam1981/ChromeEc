/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns family-specific configuration */
#include "common.h"
#include "console.h"
#include "adc.h"
#include "adc_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_pd.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

struct hpd_mark {
	int level;
	uint64_t ts;
	enum gpio_signal signal;
};

static struct hpd_mark hpd_last_event;

static int hpd_override;
/******************************************************************************/

static int board_power_sequence(void)
{
	int i;

	for(i = 0; i < BOARD_NUM_POWER_GPIOS; i++) {
		gpio_set_level(board_power_seq[i].signal,
			       board_power_seq[i].pol);
		msleep(board_power_seq[i].delay);
	}

	return EC_SUCCESS;
}

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"usbc",   I2C_PORT_USBC,   400, GPIO_EC_I2C1_SCL, GPIO_EC_I2C1_SDA},
	{"usb_mst",  I2C_PORT_MST,  400, GPIO_EC_I2C2_SCL, GPIO_EC_I2C2_SDA},
	{"eeprom",  I2C_PORT_EEPROM,  400, GPIO_EC_I2C3_SCL, GPIO_EC_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void baseboard_init(void)
{
	/* Turn on power rails */
	board_power_sequence();
	CPRINTS("board: Power rails enabled");
}
DECLARE_HOOK(HOOK_INIT, baseboard_init, HOOK_PRIO_DEFAULT);



void baseboard_trigger_hpd_irq(void)
{
	pd_send_hpd(0, hpd_irq);
	CPRINTS("hpd: irq event detected");
}
DECLARE_DEFERRED(baseboard_trigger_hpd_irq);

void baseboard_trigger_hpd_chg(void)
{
	int level = gpio_get_level(hpd_last_event.signal);

	if (level != hpd_last_event.level)
		/* It's a glitch while in deferred or canceled action */
		return;

	if (!hpd_override)
		pd_send_hpd(0, (level) ? hpd_high : hpd_low);
	CPRINTS("hpd: change detected: level = %d", level);
}
DECLARE_DEFERRED(baseboard_trigger_hpd_chg);

void baseboard_hpd_info(void)
{
	CPRINTS("hpd: edge detect: level = %ul\t ts = %llu us",
		hpd_last_event.level, hpd_last_event.ts);
}
DECLARE_DEFERRED(baseboard_hpd_info);

void baseboard_manage_hpd_event(int signal)
{
	timestamp_t t_now = get_time();
	int level = gpio_get_level(signal);
	uint64_t t_delta = t_now.val - hpd_last_event.ts;

	/*
	 * HPD to honeybuns is from the MST hub. This is same signal that MST
	 * sends to demux chip. The MST manages glitch detection, so edges on
	 * the HPD signal to EC does not require any debouncing.
	 *
	 * There are 3 HPD events which need to be detected:
	 *    1) HPD_IRQ -> low pulse where 250 uSEc < t < 2 msec
	 *    2) HPD_LOW -> high -> low change with t > 2 msec
	 *    3) HPD_HIGH -> low -> high change with t > 2 msec
	 */

	hook_call_deferred(&baseboard_hpd_info_data, 0);

	/* All previous hpd level events need to be re-triggered */
	hook_call_deferred(&baseboard_trigger_hpd_chg_data, -1);

	if ((!hpd_last_event.level && level) &&
	    (t_delta < HPD_USTREAM_DEBOUNCE_LVL))
		/* Low -> High edge && pulse_width < 2 msec = HPD_IRQ */
		hook_call_deferred(&baseboard_trigger_hpd_irq_data, 0);
	else if (t_delta >= HPD_USTREAM_DEBOUNCE_LVL)
		hook_call_deferred(&baseboard_trigger_hpd_chg_data,
				   HPD_USTREAM_DEBOUNCE_LVL);

	/* Save current info for subsequent hpd edge */
	hpd_last_event.level = level;
	hpd_last_event.ts = t_now.val;
	/* This isn't changing but needs to be set at least once */
	hpd_last_event.signal = signal;
}


static int command_hpd(int argc, char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	hpd_override = 1;
	if (!strcasecmp(argv[1], "high")) {
		pd_send_hpd(0, hpd_high);
		msleep(10);
		pd_send_hpd(0, hpd_irq);
	} else if (!strcasecmp(argv[1], "low")) {
		pd_send_hpd(0, hpd_low);
	} else if (!strcasecmp(argv[1], "irq")) {
		pd_send_hpd(0, hpd_irq);
	} else if (!strcasecmp(argv[1], "auto")) {
		hpd_override = 0;
	} else {
	}
	ccprintf("hpd: override = %d\n", hpd_override);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hpd, command_hpd,
			"[high|low|auto]",
			"Turn on/off");
