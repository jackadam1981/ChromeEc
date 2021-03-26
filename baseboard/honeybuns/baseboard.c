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
#include "task.h"
#include "timer.h"
#include "usb_tc_sm.h"
#include "usbc_ppc.h"
#include "driver/tcpm/tcpm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#define POWER_BUTTON_PRESS_SHORT_USEC (300 * MSEC)
#define POWER_BUTTON_PRESS_LONG_USEC (5000 * MSEC)
#define POWER_BUTTON_PRESS_DEBOUNCE_USEC (30)

#define BUTTON_PRESSED_LEVEL 1
#define BUTTON_RELEASED_LEVEL 0

#define BUTTON_EVT_CHANGE   BIT(0)
#define BUTTON_EVT_INFO     BIT(1)

enum power {
	POWER_OFF,
	POWER_ON
};

enum button {
	BUTTON_RELEASE,
	BUTTON_PRESS,
	BUTTON_PRESS_POWER_ON,
	BUTTON_PRESS_SHORT,
	BUTTON_PRESS_LONG,
};

enum led_color {
	GREEN,
	YELLOW,
	OFF,
};

static enum power dock_state;
#ifdef SECTION_IS_RW
static int button_level;
static int button_level_pending;
static int dock_mf;

static char press_string[][10] = {
	"Release",
	"Press",
	"Power On",
	"Short",
	"Long",
};
#endif

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

	dock_state = enable;
	CPRINTS("board: Power rails %s", dock_state ? "on" : "off");
}

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"i2c1",  I2C_PORT_I2C1,  400, GPIO_EC_I2C1_SCL, GPIO_EC_I2C1_SDA},
	{"i2c3",  I2C_PORT_I2C3,  400, GPIO_EC_I2C3_SCL, GPIO_EC_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef SECTION_IS_RW
static void baseboard_set_led(enum led_color color)
{
	CPRINTS("led: color = %d", color);
	if (color == OFF) {
		gpio_set_level(GPIO_EC_STATUS_LED1, 1);
		gpio_set_level(GPIO_EC_STATUS_LED2, 1);
	} else if (color == GREEN) {
		gpio_set_level(GPIO_EC_STATUS_LED1, 1);
		gpio_set_level(GPIO_EC_STATUS_LED2, 0);
	} else if (color == YELLOW) {
		gpio_set_level(GPIO_EC_STATUS_LED1, 0);
		gpio_set_level(GPIO_EC_STATUS_LED2, 0);
	}
}

static int led_count;

static void baseboard_led_callback(void);
DECLARE_DEFERRED(baseboard_led_callback);

static void baseboard_led_callback(void)
{
	int color = led_count & 0x4 ? dock_mf : dock_mf ^ 1;

	if (led_count & 1) {
		baseboard_set_led(color);
	} else {
		baseboard_set_led(OFF);
	}

	if (++led_count < 8) {
		hook_call_deferred(&baseboard_led_callback_data, 150 * MSEC);
	}
}

static void baseboard_change_mf_led(int mf)
{
	led_count = 0;
	CPRINTS("baseboard: new mf = %d", dock_mf);
	baseboard_led_callback();
}

static void baseboard_set_dp_lane_control(void)
{
	int rv;
	uint32_t fw_config;

	/* Set MST lane control before MST comes out of reset */
	rv = cbi_get_fw_config(&fw_config);
	if (!rv) {
		/* put MST into reset */
		gpio_set_level(GPIO_MST_RST_L, 0);
		msleep(1);
		dock_mf = fw_config & 1;
		gpio_set_level(GPIO_MST_HUB_LANE_SWITCH, dock_mf);
		CPRINTS("MST: Lane Control Init = %d",
			gpio_get_level(GPIO_MST_HUB_LANE_SWITCH));
		msleep(1);
		gpio_set_level(GPIO_MST_RST_L, 1);
	}
}
#endif

static void baseboard_init(void)
{
#ifdef SECTION_IS_RW
	uint32_t fw_config;
#endif

	/* Turn on power rails */
	board_power_sequence(1);
	CPRINTS("board: Power rails enabled");

	/*
	 * Set up host port usbc to present Rd on CC lines if in RO. Note, that
	 * in RW this only does the PPC initialization required to remove dead
	 * battery Rd from CC lines to force a usbc detach with host when
	 * jumping from RO to RW.
	 */
	if(baseboard_usbc_init(USB_PD_PORT_HOST))
		CPRINTS("usbc: Failed to set up sink path");

#ifdef SECTION_IS_RW
	/* Force TC state machine to start in TC_ERROR_RECOVERY */
	system_clear_reset_flags(EC_RESET_FLAG_POWER_ON);
	/* Make certain SN5S330 PPC does full initialization */
	system_set_reset_flags(EC_RESET_FLAG_EFS);

	if (cbi_get_fw_config(&fw_config)) {
		cbi_set_fw_config(0);
	} else {
		CPRINTS("baseboard: mf config = %d", fw_config & 0x1);
	}

	baseboard_set_dp_lane_control();
	/* Enable power button interrupt */
	gpio_enable_interrupt(GPIO_PWR_BTN);
	baseboard_set_led(dock_mf);
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
	int port_max = board_get_usb_pd_port_count();
	int port;

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
	for (port = 0; port < port_max; port++) {
		ppc_init(port);
		msleep(1000);
		/* Inform TC state machine that it can resume */
		pd_set_suspend(port, 0);
	}
	/* Enable usbc interrupts */
	board_enable_usbc_interrupts();
}

static void baseboard_power_off(void)
{
	int port_max = board_get_usb_pd_port_count();
	int port;

	/* Put ports in TC suspend state */
	for (port = 0; port < port_max; port++)
		pd_set_suspend(port, 1);

	/* Disable ucpd peripheral (prevents interrupts) */
	tcpm_release(USB_PD_PORT_HOST);
	/* Disable PPC/TCPC interrupts */
	board_disable_usbc_interrupts();
	/* Go into power off state */
	board_power_sequence(0);
}

static void baseboard_toggle_mf(void)
{
	uint32_t fw_config;

	if (!cbi_get_fw_config(&fw_config)) {
		fw_config ^= 1;
		cbi_set_fw_config(fw_config);
		dock_mf = fw_config & 1;
		baseboard_change_mf_led(dock_mf);
		baseboard_power_off();
		baseboard_power_on();
	}
}

/*
 * Main task entry point for UCPD task
 *
 * @param p The PD port number for which to handle interrupts (pointer is
 * reinterpreted as an integer directly).
 */
void power_button_task(void *u)
{
	int timer_us = POWER_BUTTON_PRESS_DEBOUNCE_USEC * 4;
	enum button state = BUTTON_RELEASE;
	uint32_t evt;

	button_level = gpio_get_level(GPIO_PWR_BTN);

	while (1) {
		evt = task_wait_event(timer_us);
		timer_us = -1;

		if (evt == BUTTON_EVT_INFO) {
			CPRINTS("pwrbtn: pwr = %d, state = %s, level = %d",
				dock_state, press_string[state], button_level);
			continue;
		}

		switch (state) {
		case BUTTON_RELEASE:
			if (button_level == BUTTON_PRESSED_LEVEL) {
				state = BUTTON_PRESS;
				timer_us = (POWER_BUTTON_PRESS_SHORT_USEC -
						 POWER_BUTTON_PRESS_DEBOUNCE_USEC);
			}
			break;
		case BUTTON_PRESS:
			if (button_level == BUTTON_RELEASED_LEVEL) {
				state = BUTTON_RELEASE;
			} else {
				state = BUTTON_PRESS_SHORT;
				timer_us = POWER_BUTTON_PRESS_LONG_USEC -
					POWER_BUTTON_PRESS_SHORT_USEC;
				if (dock_state == POWER_OFF) {
					baseboard_power_on();
				        state = BUTTON_PRESS_POWER_ON;
				}
			}
			break;
		case BUTTON_PRESS_POWER_ON:
			if (button_level == BUTTON_RELEASED_LEVEL) {
				state = BUTTON_RELEASE;
			} else {
				state = BUTTON_PRESS_LONG;
				baseboard_toggle_mf();
			}
			break;
		case BUTTON_PRESS_SHORT:
			CPRINTS("button_short: level = %d, evt = %x",
				button_level, evt);
			if (button_level == BUTTON_RELEASED_LEVEL) {
				CPRINTS("button_short: release, power off!");
				state = BUTTON_RELEASE;
				baseboard_power_off();
			} else {
				state = BUTTON_PRESS_LONG;
				CPRINTS("button_short: state -> LONG!");
				baseboard_toggle_mf();
			}
			break;
		case BUTTON_PRESS_LONG:
			if (button_level == BUTTON_RELEASED_LEVEL) {
				state = BUTTON_RELEASE;
			}
			break;
	}

	CPRINTS("power: dock = %s, level = %d,  button = %s, evt = %x, timer = %d",
		dock_state ? "on" : "off", button_level,
		press_string[state],
		evt, timer_us);

	}
}

static void baseboard_power_button_debounce(void)
{
	int level = gpio_get_level(GPIO_PWR_BTN);

	/* Sanity check, level should be same after debounce interval */
	if (level != button_level_pending)
		return;

	button_level = level;
	task_set_event(TASK_ID_POWER_BUTTON, BUTTON_EVT_CHANGE);
}
DECLARE_DEFERRED(baseboard_power_button_debounce);

void baseboard_power_button_evt(int level)
{
	button_level_pending = level;

	hook_call_deferred(&baseboard_power_button_debounce_data,
			   POWER_BUTTON_PRESS_DEBOUNCE_USEC);
}

static int command_pwr_btn(int argc, char **argv)
{

	if (argc == 1) {
		task_set_event(TASK_ID_POWER_BUTTON, BUTTON_EVT_INFO);
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "on")) {
		CPRINTS("baseboard: tc state 1 = %s",  tc_get_current_state(0));
		baseboard_power_on();
		CPRINTS("baseboard: tc state 2 = %s",  tc_get_current_state(0));
	} else if (!strcasecmp(argv[1], "off")) {
		baseboard_power_off();
	} else if (!strcasecmp(argv[1], "mf")) {
		baseboard_toggle_mf();
	} else {
		return EC_ERROR_PARAM1;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pwr_btn, command_pwr_btn,
			"<on|off>",
			"pwr btn");

#endif
