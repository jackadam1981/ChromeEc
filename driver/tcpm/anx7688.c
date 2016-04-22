/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ANX7688 port manager */

#include "hooks.h"
#include "tcpci.h"
#include "tcpm.h"

#define ANX7688_VENDOR_ALERT    (1 << 15)

#define ANX7688_REG_STATUS      0x82
#define ANX7688_REG_STATUS_LINK (1 << 0)

#define ANX7688_REG_HPD         0x83
#define ANX7688_REG_HPD_HIGH    (1 << 0)
#define ANX7688_REG_HPD_IRQ     (1 << 1)
#define ANX7688_REG_HPD_ENABLE  (1 << 2)

static int anx7688_init(int port)
{
	int rv;
	int mask;

	rv = tcpci_tcpm_drv.init(port);
	if (rv)
		return rv;

	rv = tcpc_read16(port, TCPC_REG_ALERT_MASK, &mask);
	if (rv)
		return rv;

	/* enable vendor specific alert */
	mask |= ANX7688_VENDOR_ALERT;
	rv = tcpc_write16(port, TCPC_REG_ALERT_MASK, mask);
	return rv;
}

static void anx7688_update_hpd_enable(int port)
{
	int status, reg, rv;

	rv = tcpc_read(port, ANX7688_REG_STATUS, &status);
	rv |= tcpc_read(port, ANX7688_REG_HPD, &reg);
	if (rv)
		return;

	tcpc_write(port, ANX7688_REG_HPD,
		   (status & ANX7688_REG_STATUS_LINK)
		   ? reg | ANX7688_REG_HPD_ENABLE
		   : reg & ~ANX7688_REG_HPD_ENABLE);
}

int anx7688_update_hpd_level(int port, int level)
{
	int reg, rv;

	rv = tcpc_read(port, ANX7688_REG_HPD, &reg);
	if (rv)
		return rv;

	return tcpc_write(port, ANX7688_REG_HPD,
			  level ? reg | ANX7688_REG_HPD_HIGH
				: reg & ~ANX7688_REG_HPD_HIGH);
}

int anx7688_update_hpd_irq(int port, int irq)
{
	int reg, rv;

	rv = tcpc_read(port, ANX7688_REG_HPD, &reg);
	if (rv)
		return rv;

	return tcpc_write(port, ANX7688_REG_HPD,
			  irq ? reg | ANX7688_REG_HPD_IRQ
			      : reg & ~ANX7688_REG_HPD_IRQ);
}

int anx7688_set_dp_pin_mode(int port, int pin_mode)
{
	int reg, rv;

	rv = tcpc_read(port, TCPC_REG_TCPC_CTRL, &reg);
	if (rv)
		return rv;
	rv = tcpc_write(port, TCPC_REG_CONFIG_STD_OUTPUT,
			TCPC_REG_TCPC_CTRL_POLARITY(reg) | 0x0c);
	return rv;
}

int anx7688_enable_cable_detection(int port)
{
	return tcpc_write(port, TCPC_REG_COMMAND, 0xff);
}

int anx7688_set_power_supply_ready(int port)
{
	return tcpc_write(port, TCPC_REG_COMMAND, 0x77);
}

int anx7688_power_supply_reset(int port)
{
	return tcpc_write(port, TCPC_REG_COMMAND, 0x66);
}


static void anx7688_tcpc_alert(int port)
{
	int alert, rv;

	rv = tcpc_read16(port, TCPC_REG_ALERT, &alert);
	/* process and clear alert status */
	tcpci_tcpm_drv.tcpc_alert(port);

	if (!rv && (alert & ANX7688_VENDOR_ALERT))
		anx7688_update_hpd_enable(port);
}

/* ANX7688 is compatible to TCPCI */
struct tcpm_drv anx7688_tcpm_drv;

static void anx7688_driver_init(void)
{
	anx7688_tcpm_drv = tcpci_tcpm_drv;
	anx7688_tcpm_drv.init = anx7688_init;
	anx7688_tcpm_drv.tcpc_alert = anx7688_tcpc_alert;
}
DECLARE_HOOK(HOOK_INIT, anx7688_driver_init, HOOK_PRIO_DEFAULT);
