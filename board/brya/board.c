/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "button.h"
#include "driver/ppc/nx20p348x.h"
#include "driver/ppc/syv682x_public.h"
#include "extpower.h"
#include "power.h"
#include "power_button.h"
#include "registers.h"
#include "switch.h"
#include "task.h"
#include "throttle_ap.h"
#include "usb_charge.h"
#include "usb_pd.h"

#include "gpio_list.h"

/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_ACOK_EC_OD,
	GPIO_EC_RST_ODL,
	GPIO_GSC_EC_PWR_BTN_ODL,
	GPIO_LID_OPEN_OD,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* USB-C section */

void tcpc_alert_event(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_C2_TCPC_INT_ODL:
		schedule_deferred_pd_interrupt(USBC_PORT_C0);
		schedule_deferred_pd_interrupt(USBC_PORT_C2);
		break;
	case GPIO_USB_C1_TCPC_INT_ODL:
		schedule_deferred_pd_interrupt(USBC_PORT_C1);
		break;
	default:
		break;
	}
}

void bc12_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_BC12_INT_ODL:
		task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12);
		break;
	case GPIO_USB_C1_BC12_INT_ODL:
		task_set_event(TASK_ID_USB_CHG_P1, USB_CHG_EVENT_BC12);
		break;
	case GPIO_USB_C2_BC12_INT_ODL:
		task_set_event(TASK_ID_USB_CHG_P2, USB_CHG_EVENT_BC12);
		break;
	default:
		break;
	}
}

void ppc_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_PPC_INT_ODL:
		syv682x_interrupt(USBC_PORT_C0);
		break;
	case GPIO_USB_C1_PPC_INT_ODL:
		nx20p348x_interrupt(USBC_PORT_C1);
		break;
	case GPIO_USB_C2_PPC_INT_ODL:
		syv682x_interrupt(USBC_PORT_C2);
		break;
	default:
		break;
	}
}

void retimer_interrupt(enum gpio_signal signal)
{
	switch (signal) {
	case GPIO_USB_C0_RT_INT_ODL:
		break;
	case GPIO_USB_C1_RT_INT_ODL:
		break;
	case GPIO_USB_C2_RT_INT_ODL:
		break;
	default:
		break;
	}
}
