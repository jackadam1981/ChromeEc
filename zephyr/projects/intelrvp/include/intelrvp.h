/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __INTELRVP_BOARD_H
#define __INTELRVP_BOARD_H

#include <devicetree.h>
#include <drivers/gpio.h>
#include "compiler.h"
#include "gpio_signal.h"
#include "stdbool.h"

/* RVP ID read retry count */
#define RVP_VERSION_READ_RETRY_CNT	2

#define RVP_BOARD_ID_DT DT_NODELABEL(rvp_board_id)

#define BOM_ID_PIN_COUNT DT_PROP_LEN(RVP_BOARD_ID_DT, bom_gpios)
#define FAB_ID_PIN_COUNT DT_PROP_LEN(RVP_BOARD_ID_DT, fab_gpios)
#define BOARD_ID_PIN_COUNT DT_PROP_LEN(RVP_BOARD_ID_DT, board_gpios)

#define RVP_ID_GPIO_DT_SPEC_GET(id, prop)                                  \
	{                                                                  \
		.port = DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(RVP_BOARD_ID_DT, \
							prop, id)),        \
		.pin = DT_GPIO_PIN_BY_IDX(RVP_BOARD_ID_DT, prop, id),      \
		.dt_flags =                                                \
		(gpio_dt_flags_t)DT_GPIO_FLAGS_BY_IDX(RVP_BOARD_ID_DT,     \
							prop, id),         \
	},

#define BOM_CONFIG_LIST \
	LISTIFY(BOM_ID_PIN_COUNT, RVP_ID_GPIO_DT_SPEC_GET, (), bom_gpios)

#define FAB_CONFIG_LIST \
	LISTIFY(FAB_ID_PIN_COUNT, RVP_ID_GPIO_DT_SPEC_GET, (), fab_gpios)

#define BOARD_CONFIG_LIST \
	LISTIFY(BOARD_ID_PIN_COUNT, RVP_ID_GPIO_DT_SPEC_GET, (), board_gpios)

struct gpio_dt_spec bom_id_config[] = {
	BOM_CONFIG_LIST
};

struct gpio_dt_spec fab_id_config[] = {
	FAB_CONFIG_LIST
};

struct gpio_dt_spec board_id_config[] = {
	BOARD_CONFIG_LIST
};

FORWARD_DECLARE_ENUM(tcpc_rp_value);

struct tcpc_aic_gpio_config_t {
	/* TCPC interrupt */
	enum gpio_signal tcpc_alert;
	/* PPC interrupt */
	enum gpio_signal ppc_alert;
	/* PPC interrupt handler */
	void (*ppc_intr_handler)(int port);
};
extern const struct tcpc_aic_gpio_config_t tcpc_aic_gpios[];

void board_charging_enable(int port, int enable);
void board_vbus_enable(int port, int enable);
void board_set_vbus_source_current_limit(int port, enum tcpc_rp_value rp);
void board_dc_jack_interrupt(enum gpio_signal signal);
void tcpc_alert_event(enum gpio_signal signal);
bool is_typec_port(int port);
#endif /* __INTELRVP_BOARD_H */
