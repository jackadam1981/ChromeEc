/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "tablet_mode.h"

static void nb_mode_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_nb_mode));
}
DECLARE_HOOK(HOOK_INIT, nb_mode_init, HOOK_PRIO_POST_LID);

void nb_mode_isr_debounce(void)
{
	int nb_mode_l = gpio_pin_get_dt(
		GPIO_DT_FROM_NODELABEL(gpio_soc_ec_ish_nb_mode_l));

	if (nb_mode_l) {
		tablet_set_mode(1, TABLET_TRIGGER_LID);
	} else {
		tablet_set_mode(0, TABLET_TRIGGER_LID);
	}
}
DECLARE_DEFERRED(nb_mode_isr_debounce);

void nb_mode_interrupt(enum gpio_signal signal)
{
	ARG_UNUSED(signal);
	hook_call_deferred(&nb_mode_isr_debounce_data, 0);
}
