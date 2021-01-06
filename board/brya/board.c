/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "button.h"
#include "common.h"
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

#include "gpio_list.h" /* Must come after other header files. */

/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_ACOK_EC_OD,
	GPIO_EC_RST_ODL,
	GPIO_GSC_EC_PWR_BTN_ODL,
	GPIO_LID_OPEN_OD,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);
