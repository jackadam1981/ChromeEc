/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns family-specific configuration */
#include "console.h"
#include "cros_board_info.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_pd.h"
#include "system.h"
#include "timer.h"
#include "usb_tc_sm.h"
#include "usbc_ppc.h"
#include "driver/tcpm/tcpm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#define POWER_BUTTON_PRESS_USEC (300 * MSEC)

enum button_state {
	BUTTON_RELEASE = 0,
	BUTTON_PRESS,
};

static int power_state;

/******************************************************************************/

__maybe_unused static void board_power_sequence(int enable)
{
	int i;

	if (enable) {
		for(i = 0; i < board_power_seq_count; i++) {
			gpio_set_level(board_power_seq[i].signal,
				       board_power_seq[i].level);
			CPRINTS("power seq: rail = %d", i);
			if (board_power_seq[i].delay_ms)
				msleep(board_power_seq[i].delay_ms);
		}
	} else {
		for(i = board_power_seq_count - 1; i >= 0; i--) {
			gpio_set_level(board_power_seq[i].signal,
				       !board_power_seq[i].level);
			CPRINTS("sequence[%d]: level = %d", i,
				!board_power_seq[i].level);
		}
	}

	power_state = enable;
	CPRINTS("board: Power rails %s", power_state ? "on" : "off");
}

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"i2c1",  I2C_PORT_I2C1,  400, GPIO_EC_I2C1_SCL, GPIO_EC_I2C1_SDA},
	{"i2c3",  I2C_PORT_I2C3,  400, GPIO_EC_I2C3_SCL, GPIO_EC_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef SECTION_IS_RW
static void baseboard_set_dp_lane_control(void)
{
	int rv;
	uint32_t fw_config;

	/* Set MST lane control before MST comes out of reset */
	rv = cbi_get_fw_config(&fw_config);
	if (!rv) {
		/* put MST into reset */
		gpio_set_level(GPIO_MST_RST_L, 0);
		/* wait 5 msec */
		msleep(2);
		gpio_set_level(GPIO_MST_HUB_LANE_SWITCH, fw_config & 1);
		CPRINTS("MST: Lane Control Init = %d",
			gpio_get_level(GPIO_MST_HUB_LANE_SWITCH));
		msleep(2);
		gpio_set_level(GPIO_MST_RST_L, 1);
	}
}

#else
static void baseboard_set_usbc_sink_mode(void)
{
	uint32_t cr;

	/*
	 * Bare minimum code required to enable UCPD peripheral and apply Rd to
	 * the CC lines. This is only applied in RO.
	 */
	/* Ensure that clock to UCPD is enabled */
	STM32_RCC_APB1ENR2 |= STM32_RCC_APB1ENR2_UPCD1EN;
	/* enable the peripheral */
	STM32_UCPD_CFGR1(0) |= STM32_UCPD_CFGR1_UCPDEN;
	/* Apply Rd to both CC lines */
	cr = STM32_UCPD_CR(0);
	cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
	STM32_UCPD_CR(0) = cr;

	CPRINTS("usbc: CR = 0x%x", STM32_UCPD_CR(0));
}
#endif


static void baseboard_init(void)
{

#ifdef SECTION_IS_RW
	system_clear_reset_flags(EC_RESET_FLAG_POWER_ON);
	system_set_reset_flags(EC_RESET_FLAG_EFS);
	baseboard_set_dp_lane_control();
	/* Enable power button interrupt */
	power_state = 1;
	gpio_enable_interrupt(GPIO_PWR_BTN);


#else
	/* Turn on power rails */
	board_power_sequence(1);
	baseboard_set_usbc_sink_mode();
#endif
}
/*
 * Power sequencing must run before any other chip init is attempted, so run
 * power sequencing as soon as I2C bus is initialized.
 */
DECLARE_HOOK(HOOK_INIT, baseboard_init, HOOK_PRIO_INIT_I2C + 1);

#ifdef SECTION_IS_RW
static void baseboard_power_on(void)
{
	/* Adjust system flags to full PPC init occurs */
	system_clear_reset_flags(EC_RESET_FLAG_POWER_ON);
	system_set_reset_flags(EC_RESET_FLAG_EFS);
	/* Enable power rails and release reset signals */
	board_power_sequence(1);
	/*
	 * Lane control (realtek MST) must be set prior to releasing MST
	 * reset.
	 */
	baseboard_set_dp_lane_control();
	/*
	 * When the power to the PPC is turned off, then back on, the PPC will
	 * default into dead battery mode. Dead battery resistors are disabled
	 * as part of the full ppc intializaiton sequence. This is required to
	 * force a detach event with port parter which can be attached as usbc
	 * source when honeybuns power rails are off.
	 */
	ppc_init(USB_PD_PORT_HOST);
	ppc_init(USB_PD_PORT_DP);
	/* Inform TC state machine that it can resume */
	pd_set_suspend(USB_PD_PORT_HOST, 0);
	pd_set_suspend(USB_PD_PORT_DP, 0);
	/* Enable usbc interrupts */
	board_enable_usbc_interrupts();
}

static void baseboard_power_off(void)
{
	/* Put ports in TC suspend state */
	pd_set_suspend(USB_PD_PORT_HOST, 1);
	pd_set_suspend(USB_PD_PORT_DP, 1);
	/* Disable ucpd peripheral (prevents interrupts) */
	tcpm_release(USB_PD_PORT_HOST);
	/* Disable PPC/TCPC interrupts */
	board_disable_usbc_interrupts();
	/* Go into power off state */
	board_power_sequence(0);
}


static void baseboard_power_button_valid(void)
{
	/* Sanity check, level should be pressed still */
	if (gpio_get_level(GPIO_PWR_BTN) == BUTTON_RELEASE)
		return;

	/*
	 * Button is still pressed and time is > press_threshold. If current
	 * power state is on, then turn off and vice versus.
	 */
	if (power_state)
		baseboard_power_off();
	else
		baseboard_power_on();

	CPRINTS("baseboard: power state = %s", power_state ? "on" : "off");
}
DECLARE_DEFERRED(baseboard_power_button_valid);

void baseboard_power_button_evt(int level)
{
	int callback_timer = (level == BUTTON_PRESS) ?
		POWER_BUTTON_PRESS_USEC : -1;

	hook_call_deferred(&baseboard_power_button_valid_data, callback_timer);
}

static int command_pwr_btn(int argc, char **argv)
{

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "on")) {
		CPRINTS("baseboard: tc state 1 = %s",  tc_get_current_state(0));
		baseboard_power_on();
		CPRINTS("baseboard: tc state 2 = %s",  tc_get_current_state(0));
	} else if (!strcasecmp(argv[1], "off")) {
		baseboard_power_off();
	} else {
		return EC_ERROR_PARAM1;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pwr_btn, command_pwr_btn,
			"<on|off>",
			"pwr btn");

#endif
