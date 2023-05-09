/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "ap_power/ap_power.h"
#include "base_state.h"
#include "chipset.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "tablet_mode.h"

#include <zephyr/drivers/gpio.h>
#include "gpio/gpio_int.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

/* Base detection and debouncing */
#define BASE_DETECT_EN_DEBOUNCE_US (350 * MSEC)
#define BASE_DETECT_DIS_DEBOUNCE_US (20 * MSEC)

/*
 * If the base status is unclear (i.e. not within expected ranges, read
 * the ADC value again every 500ms.
 */
#define BASE_DETECT_RETRY_US (500 * MSEC)

#define ATTACH_MAX_THRESHOLD_MV 400
#define DETACH_MIN_THRESHOLD_MV 2700

static uint64_t base_detect_debounce_time;

/*
 * Base EC pulses detection pin for 500 us to signal out of band USB wake (that
 * can be used to wake system from deep S3).
 */
#define BASE_DETECT_PULSE_MIN_US 400
#define BASE_DETECT_PULSE_MAX_US 650

static void base_detect_deferred(void);
DECLARE_DEFERRED(base_detect_deferred);

enum base_status {
	BASE_UNKNOWN = 0,
	BASE_DISCONNECTED = 1,
	BASE_CONNECTED = 2,
};

static enum base_status current_base_status;

/* Measure detection pin pulse duration (used to wake AP from deep S3). */
static uint64_t pulse_start;
static uint32_t pulse_width;

static void base_update(enum base_status attached)
{
	int connected = (attached != BASE_CONNECTED) ? false : true ;

	if (current_base_status == attached)
		return;

	current_base_status = attached;

	base_set_state(connected);
	tablet_set_mode(!connected, TABLET_TRIGGER_BASE);

	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(en_pp3300_base_x), connected);
}



void base_detect_interrupt(enum gpio_signal signal)
{
	uint64_t time_now = get_time().val;
	int pogo_prsnt = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(pogo_prsnt_int_l));
	int debounce_us;

	if (pogo_prsnt == 1)
		debounce_us = BASE_DETECT_EN_DEBOUNCE_US;
	else
		debounce_us = BASE_DETECT_DIS_DEBOUNCE_US;

	if (base_detect_debounce_time <= time_now) {
		if (current_base_status == BASE_CONNECTED &&
		!pogo_prsnt){
			pulse_start = time_now;
		} else {
			pulse_start = 0;
		}
		pulse_width = 0;

		hook_call_deferred(&base_detect_deferred_data, debounce_us);
	} else {
		if (current_base_status == BASE_CONNECTED &&
		    pogo_prsnt && !pulse_width && pulse_start) {
			pulse_width = time_now - pulse_start;
		} else {
			pulse_start = 0;
			pulse_width = 0;
		}
	}

	base_detect_debounce_time = time_now + debounce_us;
}

static void base_detect_deferred(void)
{
	uint64_t time_now = get_time().val;
	uint32_t tmp_pulse_width = pulse_width;

	if (base_detect_debounce_time > time_now) {
		hook_call_deferred(&base_detect_deferred_data,
				   base_detect_debounce_time - time_now);
		return;
	}
	int mv = adc_read_channel(ADC_BASE_DET);

	if (mv >= DETACH_MIN_THRESHOLD_MV) {
		base_update(BASE_DISCONNECTED);
	} else if (mv <= ATTACH_MAX_THRESHOLD_MV) {
		if (current_base_status != BASE_CONNECTED) {
			base_update(BASE_CONNECTED);
		} else if (tmp_pulse_width >= BASE_DETECT_PULSE_MIN_US &&
			   tmp_pulse_width <= BASE_DETECT_PULSE_MAX_US) {
			CPRINTS("Sending event to AP");
			host_set_single_event(EC_HOST_EVENT_KEY_PRESSED);
		}
	}
	else {
		host_set_single_event(EC_HOST_EVENT_KEY_PRESSED);
	}

	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(pogo_prsnt_int));
}

static void base_detect_enable(bool enable)
{
	if (enable) {
		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(pogo_prsnt_int));
	} else {
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(pogo_prsnt_int));
		base_update(BASE_DISCONNECTED);
	}
}

static void base_startup_hook(struct ap_power_ev_callback *cb,
			      struct ap_power_ev_data data)
{
	switch (data.event) {
	case AP_POWER_STARTUP:
		base_detect_enable(true);
		hook_call_deferred(&base_detect_deferred_data, BASE_DETECT_PULSE_MIN_US);
		break;
	case AP_POWER_SHUTDOWN:
		base_detect_enable(false);
		break;
	default:
		return;
	}
}

static int base_init(void)
{
	static struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb, base_startup_hook,
				  AP_POWER_STARTUP | AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);

	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		base_detect_enable(true);
	}

	return 0;
}

SYS_INIT(base_init, APPLICATION, 1);

void base_force_state(enum ec_set_base_state_cmd state)
{
	switch (state) {
	case EC_SET_BASE_STATE_ATTACH:
		base_update(BASE_CONNECTED);
		base_detect_enable(false);
		break;
	case EC_SET_BASE_STATE_DETACH:
		base_update(BASE_DISCONNECTED);
		base_detect_enable(false);
		break;
	case EC_SET_BASE_STATE_RESET:
		base_detect_enable(true);
		break;
	}
}
