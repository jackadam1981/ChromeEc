/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "board.h"
#include "charge_manager.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"
#include "usb_pd.h"
#include "util.h"

/* Krane base detection code */

/* Base detection and debouncing */
#define BASE_DETECT_DEBOUNCE_US (20 * MSEC)

/*
 * If the base status is unclear (i.e. not within expected ranges, read
 * the ADC value again every 500ms.
 */
#define BASE_DETECT_RETRY_US (500 * MSEC)

/**
 * ADC value to indicate device is attached to a keyboard.
 * 3.3V, 10K + 1K ohm => 0.3V, +10% margin.
 */
#define KEYBOARD_DETECT_MIN_MV 270
#define KEYBOARD_DETECT_MAX_MV 330

/**
 * Minimum ADC value to indicate device is attached to a dock, or disconnected.
 * 3.3V, 10K + 100K ohm => 3V, +10% margin.
 */
#define DOCK_DETECT_MIN_MV 141
#define DOCK_DETECT_MAX_MV 173

#define DETACHED_MIN_MV 2700

static uint64_t base_detect_debounce_time;

static void pogo_charge_detect(int present)
{
	if (present) {
		struct charge_port_info info = {
			.voltage = 5000, .current = 1500};
		/*
		 * Set supplier type to PD to have same priority as type c
		 * port.
		 */
		charge_manager_update_charge(
			CHARGE_SUPPLIER_DEDICATED, CHARGE_PORT_POGO, &info);
	} else {
		charge_manager_update_charge(
			CHARGE_SUPPLIER_DEDICATED, CHARGE_PORT_POGO, NULL);
	}
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

static void base_detect_deferred(void);
DECLARE_DEFERRED(base_detect_deferred);

static void base_detect_deferred(void)
{
	uint64_t time_now = get_time().val;
	int v;
	int charger_present;

	if (base_detect_debounce_time > time_now) {
		hook_call_deferred(&base_detect_deferred_data,
				   base_detect_debounce_time - time_now);
		return;
	}

	gpio_set_flags(GPIO_POGO_ADC_INT_L, GPIO_ANALOG);
	v = adc_read_channel(ADC_POGO_ADC_INT_L);
	if (v == ADC_READ_ERROR)
		goto cleanup;

	charger_present = v >= DOCK_DETECT_MIN_MV && v <= DOCK_DETECT_MAX_MV;
	pogo_charge_detect(charger_present);

	if (v >= KEYBOARD_DETECT_MIN_MV && v <= KEYBOARD_DETECT_MAX_MV) {
		gpio_set_level(GPIO_EN_PP3300_POGO, 1);
		goto cleanup;
	}

	if (charger_present || v >= DETACHED_MIN_MV)
		goto cleanup;

	/* Unclear base status, schedule again in a while. */
	hook_call_deferred(&base_detect_deferred_data, BASE_DETECT_RETRY_US);

cleanup:
	gpio_set_flags(GPIO_POGO_ADC_INT_L, GPIO_ANALOG);
}

void pogo_adc_interrupt(enum gpio_signal signal)
{
	uint64_t time_now = get_time().val;

	if (base_detect_debounce_time <= time_now) {
		hook_call_deferred(&base_detect_deferred_data,
				   BASE_DETECT_DEBOUNCE_US);
	}


	base_detect_debounce_time = time_now + BASE_DETECT_DEBOUNCE_US;
}

static void base_init(void)
{
	hook_call_deferred(&base_detect_deferred_data, 0);
}
DECLARE_HOOK(HOOK_INIT, base_init, HOOK_PRIO_DEFAULT + 1);
