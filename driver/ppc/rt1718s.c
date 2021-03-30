/* Copyright 2032 The Richtek Technology Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Richtek RT1718S USB-C Power Path Controller */
#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/ppc/rt1718s.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/tcpm.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"
#include "util.h"


#define RT1718S_FLAGS_SOURCE_ENABLED BIT(0)
static uint8_t flags[CONFIG_USB_PD_PORT_MAX_COUNT];

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)


static int read_reg(uint8_t port, int reg, int *regval)
{
    if (reg & 0xF200) {
		int rv;
		uint8_t out[2], in ;
		out[0] = (reg & 0xFF00) >>8;
		out[1] = (reg & 0x00FF);

		rv = i2c_xfer(ppc_chips[port].i2c_port,
			ppc_chips[port].i2c_addr_flags,
			out, sizeof(out), &in, sizeof(in));
		*regval = (int)in;
		return rv;
	} else {
	    return i2c_read8(ppc_chips[port].i2c_port,
               ppc_chips[port].i2c_addr_flags,
               reg,
               regval);
    }
}

static int write_reg(uint8_t port, int reg, int regval)
{
	if (reg & 0xF200) {
		uint8_t out[3];
		out[0] = (reg & 0xFF00) >>8;
		out[1] = (reg & 0x00FF);
		out[2] = regval;

		return i2c_xfer(ppc_chips[port].i2c_port,
			ppc_chips[port].i2c_addr_flags,
			out, sizeof(out), NULL, 0);
	} else {
		return i2c_write8(ppc_chips[port].i2c_port,
		       ppc_chips[port].i2c_addr_flags,
		       reg, regval);
	}
}


static int update_bits(int port, int reg, int mask, int val)
{
	int reg_val;

	if (mask == 0xFF)
		return write_reg(port, reg, val);

	RETURN_ERROR(read_reg(port, reg, &reg_val));
	reg_val &= (~mask);
	reg_val |= (mask & val);
	return write_reg(port, reg, reg_val);
}


static int rt1718s_is_sourcing_vbus(int port)
{
	return (flags[port] & RT1718S_FLAGS_SOURCE_ENABLED);
}

static int rt1718s_vbus_source_enable(int port, int enable)
{
	if (enable)
		flags[port] |= RT1718S_FLAGS_SOURCE_ENABLED;
	else
		flags[port] &= ~RT1718S_FLAGS_SOURCE_ENABLED;

	return tcpm_set_src_ctrl(port, enable);
}

static int rt1718s_vbus_sink_enable(int port, int enable)
{
	return tcpm_set_snk_ctrl(port, enable);
}

static int rt1718s_discharge_vbus(int port, int enable)
{
	return update_bits(port,
		TCPC_REG_POWER_CTRL,
		TCPC_REG_POWER_CTRL_FORCE_DISCHARGE,
		(enable) ? MASK_SET : MASK_CLR);
}

/*
static int rt1718s_set_vbus_source_current_limit(int port,
						 enum tcpc_rp_value rp)
{
	return EC_ERROR_UNIMPLEMENTED;
	return EC_SUCCESS;
}
*/

#ifdef CONFIG_CMD_PPC_DUMP
static int rt1718s_dump(int port)
{
	int reg_addr;
	int data;

	for (reg_addr = 0; reg_addr <= 0xFF; reg_addr++) {
		int rv = read_reg(port, reg_addr, &data);

		if (rv)
			ccprintf("ppc_rt1718s[p%d]: Failed to read reg 0x%02x\n",
				 port, reg_addr);
		else
			ccprintf("ppc_rt1718s[p%d]: reg 0x%02x = 0x%02x\n",
				 port, reg_addr, data);
	}
	cflush();

	return EC_SUCCESS;
}
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
static int rt1718s_is_vbus_present(int port)
{
	int status;
	int rv = read_reg(port, TCPC_REG_POWER_STATUS, &status);

	return (rv == 0) && (status & TCPC_REG_POWER_STATUS_VBUS_PRES);
}
#endif

static void rt1718s_handle_interrupt(int port)
{
	/* Do nothing */
}

void rt1718s_interrupt(int port)
{
	rt1718s_handle_interrupt(port);
}

static int rt1718s_init(int port)
{
	flags[port] = 0;

	return EC_SUCCESS;
}

#ifdef CONFIG_USBC_PPC_POLARITY
static int rt1718s_set_polarity(int port, int polarity)
{
	return tcpci_tcpm_set_polarity(port, polarity);
}
#endif

const struct ppc_drv rt1718s_ppc_drv = {
	.init = &rt1718s_init,
	.is_sourcing_vbus = &rt1718s_is_sourcing_vbus,
	.vbus_sink_enable = &rt1718s_vbus_sink_enable,
	.vbus_source_enable = &rt1718s_vbus_source_enable,
#ifdef CONFIG_CMD_PPC_DUMP
	.reg_dump = &rt1718s_dump,
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	.is_vbus_present = &rt1718s_is_vbus_present,
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */

	/* .set_vbus_source_current_limit = &rt1718s_set_vbus_source_current_limit, */ 
	.discharge_vbus = &rt1718s_discharge_vbus,

#ifdef CONFIG_USBC_PPC_POLARITY
	.set_polarity = &rt1718s_set_polarity,
#endif

#ifdef CONFIG_USBC_PPC_VCONN
	.set_vconn = &tcpci_tcpm_set_vconn,
#endif
};
