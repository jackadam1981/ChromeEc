/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Plankton board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "ina2xx.h"
#include "ioexpander_pca9534.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"

static volatile uint64_t hpd_prev_ts;
static volatile int hpd_prev_level;

void button_event(enum gpio_signal signal);
void hpd_event(enum gpio_signal signal);
void vbus_event(enum gpio_signal signal);
#include "gpio_list.h"

/**
 * Hotplug detect deferred task
 *
 * Called after level change on hpd GPIO to evaluate (and debounce) what event
 * has occurred.  There are 3 events that occur on HPD:
 *    1. low  : downstream display sink is deattached
 *    2. high : downstream display sink is attached
 *    3. irq  : downstream display sink signalling an interrupt.
 *
 * The debounce times for these various events are:
 *  100MSEC : min pulse width of level value.
 *    2MSEC : min pulse width of IRQ low pulse.  Max is level debounce min.
 *
 * lvl(n-2) lvl(n-1)  lvl   prev_delta  now_delta event
 * ----------------------------------------------------
 * 1        0         1     <2ms        n/a       low glitch (ignore)
 * 1        0         1     >2ms        <100ms    irq
 * x        0         1     n/a         >100ms    high
 * 0        1         0     <100ms      n/a       high glitch (ignore)
 * x        1         0     n/a         >100ms    low
 */

void hpd_irq_deferred(void)
{
	pd_send_hpd(0, hpd_irq);
}
DECLARE_DEFERRED(hpd_irq_deferred);

void hpd_lvl_deferred(void)
{
	int level = gpio_get_level(GPIO_DPSRC_HPD);
	if (level != hpd_prev_level)
		/* It's a glitch while in deferred or canceled action */
		return;

	pd_send_hpd(0, (level) ? hpd_high : hpd_low);
}
DECLARE_DEFERRED(hpd_lvl_deferred);

void hpd_event(enum gpio_signal signal)
{
	timestamp_t now = get_time();
	int level = gpio_get_level(signal);
	uint64_t cur_delta = now.val - hpd_prev_ts;

	/* store current time */
	hpd_prev_ts = now.val;

	/* All previous hpd level events need to be re-triggered */
	hook_call_deferred(hpd_lvl_deferred, -1);

	/* It's a glitch.  Previous time moves but level is the same. */
	if (cur_delta < HPD_DEBOUNCE_IRQ)
		return;

	if ((!hpd_prev_level && level) && (cur_delta < HPD_DEBOUNCE_LVL))
		/* It's an irq */
		hook_call_deferred(hpd_irq_deferred, 0);
	else if (cur_delta >= HPD_DEBOUNCE_LVL)
		hook_call_deferred(hpd_lvl_deferred, HPD_DEBOUNCE_LVL);

	hpd_prev_level = level;
}

/* Debounce time for voltage buttons */
#define BUTTON_DEBOUNCE_US (100 * MSEC)

static enum gpio_signal button_pressed;

enum usbc_action {
	USBC_ACT_5V_TO_DUT,
	USBC_ACT_12V_TO_DUT,
	USBC_ACT_20V_TO_DUT,
	USBC_ACT_DEVICE,
	USBC_ACT_USBDP_TOGGLE,
	USBC_ACT_USB_EN,
	USBC_ACT_DP_EN,
	USBC_ACT_MUX_FLIP,
	USBC_ACT_CABLE_POLARITY0,
	USBC_ACT_CABLE_POLARITY1,

	/* Number of USBC actions */
	USBC_ACT_COUNT
};

enum board_src_cap src_cap_mapping[USBC_ACT_COUNT] =
{
	[USBC_ACT_5V_TO_DUT] = SRC_CAP_5V,
	[USBC_ACT_12V_TO_DUT] = SRC_CAP_12V,
	[USBC_ACT_20V_TO_DUT] = SRC_CAP_20V,
};

static void set_usbc_action(enum usbc_action act)
{
	int need_soft_reset;

	switch (act) {
	case USBC_ACT_5V_TO_DUT:
	case USBC_ACT_12V_TO_DUT:
	case USBC_ACT_20V_TO_DUT:
		need_soft_reset = gpio_get_level(GPIO_VBUS_CHARGER_EN);
		board_set_source_cap(src_cap_mapping[act]);
		pd_set_dual_role(PD_DRP_FORCE_SOURCE);
		if (need_soft_reset)
			pd_soft_reset();
		break;
	case USBC_ACT_DEVICE:
		pd_set_dual_role(PD_DRP_FORCE_SINK);
		break;
	case USBC_ACT_USBDP_TOGGLE:
		gpio_set_level(GPIO_USBC_SS_USB_MODE,
			       !gpio_get_level(GPIO_USBC_SS_USB_MODE));
		break;
	case USBC_ACT_USB_EN:
		gpio_set_level(GPIO_USBC_SS_USB_MODE, 1);
		break;
	case USBC_ACT_DP_EN:
		gpio_set_level(GPIO_USBC_SS_USB_MODE, 0);
		break;
	case USBC_ACT_MUX_FLIP:
		pd_send_vdm(0, USB_VID_GOOGLE, VDO_CMD_FLIP, NULL, 0);
		gpio_set_level(GPIO_USBC_POLARITY,
			       !gpio_get_level(GPIO_USBC_POLARITY));
		break;
	case USBC_ACT_CABLE_POLARITY0:
		gpio_set_level(GPIO_USBC_POLARITY, 0);
		break;
	case USBC_ACT_CABLE_POLARITY1:
		gpio_set_level(GPIO_USBC_POLARITY, 1);
		break;
	default:
		break;
	}
}

static int prev_dbg20v;
static void button_dbg20v_deferred(void);
static void enable_dbg20v_poll(void)
{
	hook_call_deferred(button_dbg20v_deferred, 10 * MSEC);
}

/* Handle debounced button press */
static void button_deferred(void)
{
	if (button_pressed == GPIO_DBG_20V_TO_DUT_L) {
		enable_dbg20v_poll();
		if (gpio_get_level(GPIO_DBG_20V_TO_DUT_L) == prev_dbg20v)
			return;
		else
			prev_dbg20v = !prev_dbg20v;
	}
	/* bounce ? */
	if (gpio_get_level(button_pressed) != 0)
		return;

	switch (button_pressed) {
	case GPIO_DBG_5V_TO_DUT_L:
		set_usbc_action(USBC_ACT_5V_TO_DUT);
		break;
	case GPIO_DBG_12V_TO_DUT_L:
		set_usbc_action(USBC_ACT_12V_TO_DUT);
		break;
	case GPIO_DBG_20V_TO_DUT_L:
		set_usbc_action(USBC_ACT_20V_TO_DUT);
		break;
	case GPIO_DBG_CHG_TO_DEV_L:
		set_usbc_action(USBC_ACT_DEVICE);
		break;
	case GPIO_DBG_USB_TOGGLE_L:
		set_usbc_action(USBC_ACT_USBDP_TOGGLE);
		break;
	case GPIO_DBG_MUX_FLIP_L:
		set_usbc_action(USBC_ACT_MUX_FLIP);
		break;
	default:
		break;
	}

	ccprintf("Button %d = %d\n",
		 button_pressed, gpio_get_level(button_pressed));
}
DECLARE_DEFERRED(button_deferred);

void button_event(enum gpio_signal signal)
{
	button_pressed = signal;
	/* reset debounce time */
	hook_call_deferred(button_deferred, BUTTON_DEBOUNCE_US);
}

static void button_dbg20v_deferred(void)
{
	if (gpio_get_level(GPIO_DBG_20V_TO_DUT_L) == 0)
		button_event(GPIO_DBG_20V_TO_DUT_L);
	else
		enable_dbg20v_poll();
}
DECLARE_DEFERRED(button_dbg20v_deferred);

void vbus_event(enum gpio_signal signal)
{
	ccprintf("VBUS! =%d\n", gpio_get_level(signal));
	task_wake(TASK_ID_PD);
}

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"CC1_PD", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_CH_CC2_PD] = {"CC2_PD", 3300, 4096, 0, STM32_AIN(4)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master",  I2C_PORT_MASTER, 100,
		GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void board_init(void)
{
	timestamp_t now = get_time();
	hpd_prev_level = gpio_get_level(GPIO_DPSRC_HPD);
	hpd_prev_ts = now.val;
	gpio_enable_interrupt(GPIO_DPSRC_HPD);

	/* Enable interrupts on VBUS transitions. */
	gpio_enable_interrupt(GPIO_VBUS_WAKE);

	/* Enable button interrupts. */
	gpio_enable_interrupt(GPIO_DBG_5V_TO_DUT_L);
	gpio_enable_interrupt(GPIO_DBG_12V_TO_DUT_L);
	gpio_enable_interrupt(GPIO_DBG_CHG_TO_DEV_L);
	gpio_enable_interrupt(GPIO_DBG_USB_TOGGLE_L);
	gpio_enable_interrupt(GPIO_DBG_MUX_FLIP_L);

	/* TODO(crosbug.com/33761): poll DBG_20V_TO_DUT_L */
	enable_dbg20v_poll();

	ina2xx_init(0, 0x399f, INA2XX_CALIB_1MA(10 /* mOhm */));

}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static int cmd_usbc_action(int argc, char *argv[])
{
	enum usbc_action act;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "5v"))
		act = USBC_ACT_5V_TO_DUT;
	else if (!strcasecmp(argv[1], "12v"))
		act = USBC_ACT_12V_TO_DUT;
	else if (!strcasecmp(argv[1], "20v"))
		act = USBC_ACT_20V_TO_DUT;
	else if (!strcasecmp(argv[1], "dev"))
		act = USBC_ACT_DEVICE;
	else if (!strcasecmp(argv[1], "usb"))
		act = USBC_ACT_USB_EN;
	else if (!strcasecmp(argv[1], "dp"))
		act = USBC_ACT_DP_EN;
	else if (!strcasecmp(argv[1], "flip"))
		act = USBC_ACT_MUX_FLIP;
	else if (!strcasecmp(argv[1], "pol0"))
		act = USBC_ACT_CABLE_POLARITY0;
	else if (!strcasecmp(argv[1], "pol1"))
		act = USBC_ACT_CABLE_POLARITY1;
	else
		return EC_ERROR_PARAM1;

	set_usbc_action(act);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(usbc_action, cmd_usbc_action,
			"<5v|12v|20v|dev|usb|dp|flip|pol0|pol1>",
			"Set Plankton type-C port state",
			NULL);

static int board_usb_hub_reset(void)
{
	int ret;

	ret = pca9534_config_pin(I2C_PORT_MASTER, 0x40, 7, PCA9534_OUTPUT);
	if (ret)
		return ret;
	ret = pca9534_set_level(I2C_PORT_MASTER, 0x40, 7, 0);
	if (ret)
		return ret;
	usleep(100 * MSEC);
	return pca9534_set_level(I2C_PORT_MASTER, 0x40, 7, 1);
}

static int cmd_usb_hub_reset(int argc, char *argv[])
{
	return board_usb_hub_reset();
}
DECLARE_CONSOLE_COMMAND(hub_reset, cmd_usb_hub_reset,
			NULL, "Reset USB hub", NULL);

static void board_usb_hub_reset_no_return(void)
{
	board_usb_hub_reset();
}
DECLARE_DEFERRED(board_usb_hub_reset_no_return);

static void board_init_usb_hub(void)
{
	if (system_get_reset_flags() & RESET_FLAG_POWER_ON)
		hook_call_deferred(board_usb_hub_reset_no_return, 500 * MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_init_usb_hub, HOOK_PRIO_DEFAULT);

