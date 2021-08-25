/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/ln9310.h"
#include "tcpm/ps8xxx_public.h"
#include "gpio.h"
#include "hooks.h"
#include "sku.h"
#include "system.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static uint8_t sku_id;

enum board_model {
	PAZQUEL,
	UNKNOWN,
};

static const char *const model_name[] = {
	"PAZQUEL",
	"UNKNOWN",
};

static enum board_model get_model(void)
{
	if (sku_id == 0 || sku_id == 1 || sku_id == 2)
		return PAZQUEL;
	return UNKNOWN;
}

/*
 * Read SKU ID from GPIO and initialize variables for board variants
 * SKU ID 0 - WiFi+LTE
 * SKU ID 1 - Wifi only
 * SKU ID 2 - WiFi+LTE w/o e-SIM
 */
static void sku_init(void)
{
	sku_id = system_get_sku_id();
	CPRINTS("SKU: %u (%s)", sku_id, model_name[get_model()]);
}
DECLARE_HOOK(HOOK_INIT, sku_init, HOOK_PRIO_INIT_I2C + 1);

int board_is_clamshell(void)
{
	return get_model() == PAZQUEL;
}
