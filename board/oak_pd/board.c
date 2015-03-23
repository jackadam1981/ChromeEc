/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* oak_pd board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "battery.h"
#include "charge_manager.h"
#include "charge_ramp.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "pi3usb9281.h"
#include "power.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "usb.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* Amount to offset the input current limit when sending to EC */
#define INPUT_CURRENT_LIMIT_OFFSET_MA 192

/* Default input current limit when VBUS is present */
#define DEFAULT_CURR_LIMIT            500  /* mA */

/*
 * When battery is high, system may not be pulling full current. Also, when
 * high AND input voltage is below boost bypass, then limit input current
 * limit to HIGH_BATT_LIMIT_CURR_MA to reduce audible ringing.
 */
#define HIGH_BATT_THRESHOLD 90
#define HIGH_BATT_LIMIT_BOOST_BYPASS_MV 11000
#define HIGH_BATT_LIMIT_CURR_MA 2000

/* Chipset power state */
static enum power_state ps;

/* Battery state of charge */
static int batt_soc;

/* Default to 5V charging allowed for dead battery case */
static enum pd_charge_state charge_state = PD_CHARGE_5V;

/*
 * PD MCU status and host event status for host command
 * Note: these vars must be aligned on 4-byte boundary because we pass the
 * address to atomic_ functions which use assembly to access them.
 */
static struct ec_response_pd_status pd_status __aligned(4);
static struct ec_response_host_event_status host_event_status __aligned(4);
struct ec_response_pd_port pd_port_status[PD_PORT_COUNT] __aligned(4);

/* Desired input current limit */
static int desired_charge_rate_ma = -1;

void pd_send_ec_int(void)
{
	gpio_set_level(GPIO_EC_INT, 1);

	/*
	 * Delay long enough to guarantee EC see's the change. Slowest
	 * EC clock speed is 250kHz in deep sleep -> 4us, and add 1us
	 * for buffer.
	 */
	usleep(5);

	gpio_set_level(GPIO_EC_INT, 0);
}

void vbus0_evt(enum gpio_signal signal)
{
#ifdef HAS_TASK_PD_C0
	task_wake(TASK_ID_PD_C0);
#endif
}

void vbus1_evt(enum gpio_signal signal)
{
#ifdef HAS_TASK_PD_C1
	task_wake(TASK_ID_PD_C1);
#endif
}

/* Charge manager callback function, called on delayed override timeout */
void board_charge_manager_override_timeout(void)
{
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}
DECLARE_DEFERRED(board_charge_manager_override_timeout);

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
	/*
	 * the DMA mapping is :
	 *  Chan 2 : TIM1_CH1  (C0 RX)
	 *  Chan 3 : SPI1_TX   (C0 TX)
	 *  Chan 4 : TIM3_CH1  (C1 RX)
	 *  Chan 5 : SPI2_TX   (C1 TX)
	 */
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	int pd_enable = 1;

	/* Enable interrupts on VBUS transitions. */
	gpio_enable_interrupt(GPIO_USB_C0_VBUS_WAKE_L);
	gpio_enable_interrupt(GPIO_USB_C1_VBUS_WAKE_L);

	/* OAK_PD: TODO: Power management of ARM based system */
	disable_sleep(SLEEP_MASK_AP_RUN);
	hook_notify(HOOK_CHIPSET_RESUME);
	ps = POWER_S0;

	/* Initialize active charge port to none */
	pd_status.active_charge_port = CHARGE_PORT_NONE;
	pd_comm_enable(pd_enable);

	/* ADC sampling time : 239.5 ADC clock cycles, about 17us */
	STM32_ADC_SMPR = 7;
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_C1_CC1_PD] = {"C1_CC1_PD", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_C_REF]     = {"C_REF    ", 3300, 4096, 0, STM32_AIN(1)},
	[ADC_C0_CC1_PD] = {"C0_CC1_PD", 3300, 4096, 0, STM32_AIN(2)},
	[ADC_C0_CC2_PD] = {"C0_CC2_PD", 3300, 4096, 0, STM32_AIN(4)},
	[ADC_C1_CC2_PD] = {"C1_CC2_PD", 3300, 4096, 0, STM32_AIN(5)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"slave",  I2C_PORT_SLAVE, 100,
		GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

void board_set_usb_mux(int port, enum typec_mux mux, int polarity)
{
	if (port >= PD_PORT_COUNT)
		return;

	pd_port_status[port].mux = mux;
	pd_port_status[port].polarity = polarity;

	CPRINTS("Set mux: %d, polarity: %d", mux, polarity);
	/* Call EC to setup the MUX */
	pd_send_ec_int();
}

int board_get_usb_mux(int port, const char **dp_str, const char **usb_str)
{
	/* Keep the mux information for EC */
	if (port >= PD_PORT_COUNT)
		return -1;
	return pd_port_status[port].mux;
}

void board_flip_usb_mux(int port)
{
	if (port >= PD_PORT_COUNT)
		return;

	pd_port_status[port].polarity = !pd_port_status[port].polarity;

	/* Call EC to setup the MUX */
	pd_send_ec_int();
}

int board_get_battery_soc(void)
{
	return batt_soc;
}

enum battery_present battery_is_present(void)
{
	if (batt_soc >= 0)
		return BP_YES;
	return BP_NOT_SURE;
}

/**
 * Return if max voltage charging is allowed.
 */
int pd_is_max_request_allowed(void)
{
	return charge_state == PD_CHARGE_MAX;
}

/**
 * Return the maximum allowed input current
 */
int board_get_ramp_current_limit(int supplier, int sup_curr)
{
	switch (supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
		return 2000;
	case CHARGE_SUPPLIER_BC12_SDP:
		return 1000;
	case CHARGE_SUPPLIER_BC12_CDP:
	case CHARGE_SUPPLIER_PROPRIETARY:
		return sup_curr;
	default:
		return 500;
	}
}

/**
 * Return if board is consuming full amount of input current
 */
int board_is_consuming_full_charge(void)
{
	return batt_soc >= 1 && batt_soc < HIGH_BATT_THRESHOLD;
}

static int board_update_charge_limit(int charge_ma)
{
	/* OAK_PD: move to EC */
	return 1;
}

/**
 * Set the charge limit based upon desired maximum.
 *
 * @param charge_ma     Desired charge limit (mA).
 */
void board_set_charge_limit(int charge_ma)
{
	/* Update current limit and notify EC if it changed */
	if (board_update_charge_limit(charge_ma))
		pd_send_ec_int();
}

static void board_update_battery_soc(int soc)
{
	batt_soc = soc;
	board_update_charge_limit(desired_charge_rate_ma);
}

/* Send host event up to AP */
void pd_send_host_event(int mask)
{
	/* mask must be set */
	if (!mask)
		return;

	atomic_or(&(host_event_status.status), mask);
	atomic_or(&(pd_status.status), PD_STATUS_HOST_EVENT);
	pd_send_ec_int();
}

/****************************************************************************/
/* Console commands */
static int command_ec_int(int argc, char **argv)
{
	pd_send_ec_int();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ecint, command_ec_int,
			"",
			"Toggle EC interrupt line",
			NULL);

static int command_pd_host_event(int argc, char **argv)
{
	int event_mask;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	event_mask = strtoi(argv[1], &e, 10);
	if (*e)
		return EC_ERROR_PARAM1;

	pd_send_host_event(event_mask);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pdevent, command_pd_host_event,
			"event_mask",
			"Send PD host event",
			NULL);

/****************************************************************************/
/* Host commands */
static int ec_status_host_cmd(struct host_cmd_handler_args *args)
{
	const struct ec_params_pd_status *p = args->params;
	struct ec_response_pd_status *r = args->response;

	/* update battery soc */
	board_update_battery_soc(p->batt_soc);

	if (args->version == 1) {
		if (p->charge_state != charge_state) {
			switch (p->charge_state) {
			case PD_CHARGE_NONE:
				/*
				 * No current allowed in, set new power request
				 * so that PD negotiates down to vSafe5V.
				 */
				charge_state = p->charge_state;
#ifdef CONFIG_CHARGE_MANAGER
				pd_set_new_power_request(
					pd_status.active_charge_port);
#endif
				/*
				 * Wake charge ramp task so that it will check
				 * board_is_vbus_too_low() and stop ramping up.
				 */
				/* task_wake(TASK_ID_CHG_RAMP); */
				CPRINTS("Chg: None");
				break;
			case PD_CHARGE_5V:
				/* Allow current on the active charge port */
				charge_state = p->charge_state;
				CPRINTS("Chg: 5V");
				break;
			case PD_CHARGE_MAX:
				/*
				 * Allow negotiation above vSafe5V. Should only
				 * ever get this command when 5V charging is
				 * already allowed.
				 */
				if (charge_state == PD_CHARGE_5V) {
#ifdef CONFIG_CHARGE_MANAGER
					charge_state = p->charge_state;
					pd_set_new_power_request(
						pd_status.active_charge_port);
					CPRINTS("Chg: Max");
#endif
				}
				break;
			default:
				break;
			}
		}
	} else {
		/*
		 * If the EC is using this command version, then it won't ever
		 * set charging allowed, so we should just assume charging at
		 * the max is allowed.
		 */
#ifdef CONFIG_CHARGE_MANAGER
		charge_state = PD_CHARGE_MAX;
		pd_set_new_power_request(pd_status.active_charge_port);
		CPRINTS("Chg: Max");
#endif
	}

	*r = pd_status;

	/* Clear host event */
	atomic_clear(&(pd_status.status), PD_STATUS_HOST_EVENT);

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_EXCHANGE_STATUS, ec_status_host_cmd,
			EC_VER_MASK(0) | EC_VER_MASK(1));

/* Host commands */
static int ec_get_pd_port_host_cmd(struct host_cmd_handler_args *args)
{
	const struct ec_params_pd_port *p = args->params;
	struct ec_response_pd_port *r = args->response;

	if (args->version == 1)
		CPRINTS("Host cmd ver 1\n");

	*r = pd_port_status[p->port];

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_GET_PORT_STATUS, ec_get_pd_port_host_cmd,
			EC_VER_MASK(0) | EC_VER_MASK(1));

static int host_event_status_host_cmd(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_status *r = args->response;

	/* Clear host event bit to avoid sending more unnecessary events */
	atomic_clear(&(pd_status.status), PD_STATUS_HOST_EVENT);

	/* Read and clear the host event status to return to AP */
	r->status = atomic_read_clear(&(host_event_status.status));

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_HOST_EVENT_STATUS, host_event_status_host_cmd,
			EC_VER_MASK(0));
