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

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static uint8_t sku_id;

enum board_model {
	HEROBRINE,
	UNKNOWN,
};

static const char *const model_name[] = {
	"HEROBRINE",
	"UNKNOWN",
};

static enum board_model get_model(void)
{
	/* Currently herobrine board has different SKU variants whose sku
	 * ids are not yet defined. Considering all sku ids to be herobrine
	 * only. Later code can be changed to allow other board with ids.
	 */
	if (sku_id == 0 || sku_id == 1 || sku_id == 2 || sku_id == 3 ||
			sku_id == 4 || sku_id == 5 || sku_id == 6)
		return HEROBRINE;
	return UNKNOWN;
}

/* Read SKU ID from GPIO and initialize variables for board variants */
static void sku_init(void)
{
	uint8_t val = 0;

	if (gpio_get_level(GPIO_SKU_ID0))
		val |= 0x01;
	if (gpio_get_level(GPIO_SKU_ID1))
		val |= 0x02;
	if (gpio_get_level(GPIO_SKU_ID2))
		val |= 0x04;

	sku_id = val;
	CPRINTS("SKU: %u (%s)", sku_id, model_name[get_model()]);
}
DECLARE_HOOK(HOOK_INIT, sku_init, HOOK_PRIO_INIT_I2C + 1);

enum battery_cell_type board_get_battery_cell_type(void)
{
	switch (get_model()) {
	case HEROBRINE:
		return BATTERY_CELL_TYPE_2S;
	default:
		return BATTERY_CELL_TYPE_UNKNOWN;
	}
}

/* Keeping this from Lazor board to add other boards in future. */
int board_is_clamshell(void)
{
	return 0;
}

__override uint16_t board_get_ps8xxx_product_id(int port)
{
	if (get_model() == HEROBRINE)
		return PS8805_PRODUCT_ID;
	return PS8805_PRODUCT_ID;
}
