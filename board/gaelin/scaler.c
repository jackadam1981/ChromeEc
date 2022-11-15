/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h"
#include "gpio.h"
#include "i2c.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

/*
 * TODO: Need to add the display mode function.
 * How to implementation?
 */
void disp_mode_interrupt(enum gpio_signal signal)
{
}

/*
 * TODO: Need to add the HDMI-in function.
 * How to implementation?
 */
void hdmi_5v_in_interrupt(enum gpio_signal signal)
{
}

void hdmi0_cable_det_interrupt(enum gpio_signal signal)
{
}

/*
 * TODO: Need to add the long press function.
 * How to implementation?
 */
void osd_int_interrupt(enum gpio_signal signal)
{
}
