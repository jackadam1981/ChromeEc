/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * STUB driver for USB-C Power Path Controller
 * The intended use case for this driver is for boards which required PPC on at
 * least 1 port, but do not have a PPC on all ports.
 */
#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/ppc/stub.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"
#include "usb_pd.h"
#include "util.h"



#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)


static int ppc_stub_is_sourcing_vbus(int port)
{
	return EC_SUCCESS;
}

static int ppc_stub_discharge_vbus(int port, int enable)
{
	/*
	 * Smart discharge mode is enabled, nothing to do
	 */
	return EC_SUCCESS;
}


static int ppc_stub_vbus_sink_enable(int port, int enable)
{

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
static int ppc_stub_is_vbus_present(int port)
{
	int vbus;

	vbus = tcpm_check_vbus_level(port, VBUS_PRESENT);
	CPRINTS("ppc_stub: vbus = %d", vbus);

	return vbus;
}
#endif

static int ppc_stub_vbus_source_enable(int port, int enable)
{
	return EC_SUCCESS;
}

static int ppc_stub_set_vbus_src_current_limit(int port,
						 enum tcpc_rp_value rp)
{
	return EC_SUCCESS;
}

#ifdef CONFIG_USBC_PPC_POLARITY
static int ppc_stub_set_polarity(int port, int polarity)
{
	return EC_SUCCESS;
}
#endif

#ifdef CONFIG_USBC_PPC_VCONN
static int ppc_stub_set_vconn(int port, int enable)
{

	return EC_SUCCESS;
}
#endif

#ifdef CONFIG_CMD_PPC_DUMP
static int ppc_stub_dump(int port)
{

	return EC_SUCCESS;
}
#endif /* defined(CONFIG_CMD_PPC_DUMP) */


static int ppc_stub_init(int port)
{

	return EC_SUCCESS;
}

const struct ppc_drv ppc_stub_drv = {
	.init = &ppc_stub_init,
	.is_sourcing_vbus = &ppc_stub_is_sourcing_vbus,
	.vbus_sink_enable = &ppc_stub_vbus_sink_enable,
	.vbus_source_enable = &ppc_stub_vbus_source_enable,
#ifdef CONFIG_CMD_PPC_DUMP
	.reg_dump = &ppc_stub_dump,
#endif /* defined(CONFIG_CMD_PPC_DUMP) */
#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	.is_vbus_present = &ppc_stub_is_vbus_present,
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */
	.set_vbus_source_current_limit = &ppc_stub_set_vbus_src_current_limit,
	.discharge_vbus = &ppc_stub_discharge_vbus,
#ifdef CONFIG_USBC_PPC_POLARITY
	.set_polarity = &ppc_stub_set_polarity,
#endif
#ifdef CONFIG_USBC_PPC_VCONN
	.set_vconn = &ppc_stub_set_vconn,
#endif
};
