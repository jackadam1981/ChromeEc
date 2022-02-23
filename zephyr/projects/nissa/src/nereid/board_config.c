/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nereid sub-board hardware configuration */

#include <drivers/gpio.h>
#include <init.h>
#include <kernel.h>
#include <sys/printk.h>

#include "driver/charger/sm5803.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "task.h"
#include "usb_charge.h"
#include "usb_pd.h"

#include "sub_board.h"

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/*
 * The USB-C1 interrupt line is shared between BC1.2, TCPC and charger.
 *
 * Because the shared IRQ may be asserted by any of the three devices, edges
 * can be lost if one device asserts the IRQ while another is still pending
 * and the IRQ can't be level-triggered because much of the processing happens
 * outside ISR context (such as in the PD interrupt thread).
 *
 * To avoid losing interrupts, we schedule a task to poll the IRQ some time
 * after we handle an event and run the ISRs again. As long as the IRQ is
 * asserted, we should continue to process events but with increased latency.
 */
static void poll_c1_line(struct k_work *unused);

#define USB_C1_IRQ GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl)
static K_WORK_DELAYABLE_DEFINE(poll_c1_work, poll_c1_line);

static void notify_c1_chips(void)
{
	sm5803_interrupt(1);					/* charger */
	schedule_deferred_pd_interrupt(1);			/* TCPC */
	task_set_event(TASK_ID_USB_CHG_P1, USB_CHG_EVENT_BC12); /* BC1.2 */
	/* Schedule a check in a bit for any lost edges */
	k_work_reschedule(&poll_c1_work, K_MSEC(5));
}

static void poll_c1_line(struct k_work *unused)
{
	/*
	 * If line is still being asserted, run the ISRs again to try to clear
	 * the IRQ.
	 */
	if (gpio_pin_get_dt(USB_C1_IRQ)) {
		notify_c1_chips();
	}
}

static void usb_c1_interrupt(const struct device *unused_device,
			     struct gpio_callback *unused_cb,
			     gpio_port_pins_t unused_pins)
{
	/* Notify all chips using this line that an interrupt came in */
	notify_c1_chips();
}

static void nereid_subboard_init(void)
{
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	/*
	 * Need to initialise board specific GPIOs since the
	 * common init code does not know about them.
	 * Remove once common code initialises all GPIOs, not just
	 * the ones with enum-names.
	 *
	 * TODO(b/214858346): Enable power after AP startup.
	 */
	if (sb != NISSA_SB_C_A && sb != NISSA_SB_HDMI_A) {
		/* Turn off unused USB A1 GPIOs */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(gpio_sub_usb_a1_ilimit_sdp),
			GPIO_DISCONNECTED);
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_en_usb_a1_vbus),
			GPIO_DISCONNECTED);
	}
	if (sb == NISSA_SB_C_A || sb == NISSA_SB_C_LTE) {
		/* Enable type-C port 1 */
		gpio_pin_configure_dt(USB_C1_IRQ, GPIO_INPUT | GPIO_ACTIVE_LOW);
		/* Configure type-A port 1 VBUS, initialise it as low */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_en_usb_a1_vbus),
			GPIO_OUTPUT_LOW);
	}
	if (sb == NISSA_SB_HDMI_A) {
		/* Disable I2C_PORT_USB_C1_TCPC */
		/* TODO(b:212490923): Use pinctrl to switch from I2C */
		/* Enable HDMI GPIOs */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_en_rails_odl),
			GPIO_OUTPUT | GPIO_OUTPUT_INIT_HIGH);
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_hdmi_en_odl),
			GPIO_OUTPUT | GPIO_OUTPUT_INIT_HIGH);
		/* Configure the interrupt separately */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_hpd_odl),
			GPIO_INPUT);
	}
}

/*
 * Run sub-board configuration.
 */
static void board_init(void)
{
	nereid_subboard_init();

	/*
	 * Enable USB-C interrupts.
	 */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0));
	if (board_get_usb_pd_port_count() == 2) {
		int rv;
		static struct gpio_callback c1_callback;
		gpio_init_callback(
			&c1_callback, usb_c1_interrupt, BIT(USB_C1_IRQ->pin));

		rv = gpio_add_callback(USB_C1_IRQ->port, &c1_callback);
		__ASSERT(rv == 0, "C1 ISR config failed with code %d", rv);
		rv = gpio_pin_interrupt_configure_dt(
			USB_C1_IRQ, GPIO_INT_ENABLE | GPIO_INT_EDGE_TO_ACTIVE);
		__ASSERT(rv == 0, "C1 IRQ config failed with code %d", rv);

		/*
		 * The IRQ line may already be asserted, so schedule a poll
		 * for once initialization is done (taking care not to race with
		 * other system init).
		 */
		k_work_reschedule(&poll_c1_work, K_NO_WAIT);
	}
}
/*
 * This depends on EC initialization which runs in main() after SYS_INIT, so
 * needs to be a EC hook rather than zephyr SYS_INIT.
 */
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

__override void board_hibernate(void)
{
	/* Shut down the chargers */
	if (board_get_usb_pd_port_count() == 2)
		sm5803_hibernate(CHARGER_SECONDARY);
	sm5803_hibernate(CHARGER_PRIMARY);
	LOG_INF("Charger(s) hibernated");
	cflush();
}

/* Trigger shutdown by enabling the Z-sleep circuit */
__override void board_hibernate_late(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_slp_z), 1);
	/*
	 * The system should hibernate, but there may be
	 * a small delay, so return.
	 */
}
