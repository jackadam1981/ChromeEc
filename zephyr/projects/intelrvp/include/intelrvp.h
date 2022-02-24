/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __INTELRVP_BOARD_H
#define __INTELRVP_BOARD_H

#include "compiler.h"
#include "config.h"
#include "stdbool.h"

/* RVP ID read retry count */
#define RVP_VERSION_READ_RETRY_CNT      2

FORWARD_DECLARE_ENUM(tcpc_rp_value);

/* TCPC gpios */
struct tcpc_gpio_t {
	enum gpio_signal pin;
	uint8_t pin_pol;
};

/* VCONN gpios */
struct vconn_gpio_t {
	enum gpio_signal cc1_pin;
	enum gpio_signal cc2_pin;
	uint8_t pin_pol;
};

struct tcpc_gpio_config_t {
	/* VBUS interrput */
	struct tcpc_gpio_t vbus;
	/* Source enable */
	struct tcpc_gpio_t src;
	/* Sink enable */
	struct tcpc_gpio_t snk;
#if defined(CONFIG_USBC_VCONN) && defined(CHIP_FAMILY_IT83XX)
	/* Enable VCONN */
	struct vconn_gpio_t vconn;
#endif
	/* Enable source ILIM */
	struct tcpc_gpio_t src_ilim;
};
extern const struct tcpc_gpio_config_t tcpc_gpios[];

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
int ioexpander_read_intelrvp_version(int *port0, int *port1);
void board_dc_jack_interrupt(enum gpio_signal signal);
void tcpc_alert_event(enum gpio_signal signal);
bool is_typec_port(int port);
#endif /* __INTELRVP_BOARD_H */
