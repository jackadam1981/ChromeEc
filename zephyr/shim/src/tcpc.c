/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usbc/tcpc_anx7447.h"
#include "usbc/tcpc_anx7447_emul.h"
#include "usbc/tcpc_ccgxxf.h"
#include "usbc/tcpc_fusb302.h"
#include "usbc/tcpc_generic_emul.h"
#include "usbc/tcpc_it8xxx2.h"
#include "usbc/tcpc_nct38xx.h"
#include "usbc/tcpc_ps8xxx.h"
#include "usbc/tcpc_ps8xxx_emul.h"
#include "usbc/tcpc_raa489000.h"
#include "usbc/tcpc_rt1718s.h"
#include "usbc/tcpci.h"
#include "usbc/utils.h"

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#define HAS_TCPC_PROP(usbc_id) \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, tcpc), (|| 1), ())

#define DT_HAS_TCPC (0 DT_FOREACH_STATUS_OKAY(named_usbc_port, HAS_TCPC_PROP))

#if DT_HAS_TCPC

#define TCPC_CHIP_ENTRY(usbc_id, tcpc_id, config_fn) \
	[USBC_PORT_NEW(usbc_id)] = config_fn(tcpc_id)

#define CHECK_COMPAT(compat, usbc_id, tcpc_id, config_fn) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(tcpc_id, compat),  \
		    (TCPC_CHIP_ENTRY(usbc_id, tcpc_id, config_fn)), ())

#ifdef TEST_BUILD
#define TCPC_CHIP_FIND_EMUL(usbc_id, tcpc_id)               \
	CHECK_COMPAT(TCPCI_EMUL_COMPAT, usbc_id, tcpc_id,   \
		     TCPC_CONFIG_TCPCI_EMUL)                \
	CHECK_COMPAT(PS8XXX_EMUL_COMPAT, usbc_id, tcpc_id,  \
		     TCPC_CONFIG_PS8XXX_EMUL)               \
	CHECK_COMPAT(ANX7447_EMUL_COMPAT, usbc_id, tcpc_id, \
		     TCPC_CONFIG_ANX7447_EMUL)
#else
#define TCPC_CHIP_FIND_EMUL(...)
#endif /* TEST_BUILD */

#define TCPC_CHIP_FIND(usbc_id, tcpc_id)                                       \
	CHECK_COMPAT(ANX7447_TCPC_COMPAT, usbc_id, tcpc_id,                    \
		     TCPC_CONFIG_ANX7447)                                      \
	CHECK_COMPAT(CCGXXF_TCPC_COMPAT, usbc_id, tcpc_id, TCPC_CONFIG_CCGXXF) \
	CHECK_COMPAT(FUSB302_TCPC_COMPAT, usbc_id, tcpc_id,                    \
		     TCPC_CONFIG_FUSB302)                                      \
	CHECK_COMPAT(IT8XXX2_TCPC_COMPAT, usbc_id, tcpc_id,                    \
		     TCPC_CONFIG_IT8XXX2)                                      \
	CHECK_COMPAT(PS8XXX_COMPAT, usbc_id, tcpc_id, TCPC_CONFIG_PS8XXX)      \
	CHECK_COMPAT(NCT38XX_TCPC_COMPAT, usbc_id, tcpc_id,                    \
		     TCPC_CONFIG_NCT38XX)                                      \
	CHECK_COMPAT(RAA489000_TCPC_COMPAT, usbc_id, tcpc_id,                  \
		     TCPC_CONFIG_RAA489000)                                    \
	CHECK_COMPAT(RT1718S_TCPC_COMPAT, usbc_id, tcpc_id,                    \
		     TCPC_CONFIG_RT1718S)                                      \
	CHECK_COMPAT(TCPCI_COMPAT, usbc_id, tcpc_id, TCPC_CONFIG_TCPCI)        \
	TCPC_CHIP_FIND_EMUL(usbc_id, tcpc_id)

#define TCPC_CHIP(usbc_id)                           \
	COND_CODE_1(DT_NODE_HAS_PROP(usbc_id, tcpc), \
		    (TCPC_CHIP_FIND(usbc_id, DT_PHANDLE(usbc_id, tcpc))), ())

#define MAYBE_CONST \
	COND_CODE_1(CONFIG_PLATFORM_EC_USB_PD_TCPC_RUNTIME_CONFIG, (), (const))

/* Type C Port Controllers */
MAYBE_CONST struct tcpc_config_t tcpc_config[] = { DT_FOREACH_STATUS_OKAY(
	named_usbc_port, TCPC_CHIP) };

struct gpio_callback int_gpio_cb[DT_NUM_INST_STATUS_OKAY(named_usbc_port)];

static void tcpc_int_gpio_callback(const struct device *dev,
				   struct gpio_callback *cb, uint32_t pins)
{
	/*
	 * Retrieve the array index from the callback pointer, and
	 * use that to get the port number.
	 */
	int port = cb - &int_gpio_cb[0];

	schedule_deferred_pd_interrupt(port);
}

/*
 * Enable all tcpc interrupts from devicetree bindings.
 * Check whether the callback is already installed, and if
 * not, init and add the callback before enabling the
 * interrupt.
 */
void tcpc_enable_interrupt(void)
{
	gpio_flags_t flags;

	for (int i = 0; i < ARRAY_SIZE(tcpc_config); i++) {
		/*
		 * Check whether callback has been initialised and interrupt
		 * pin has been configured by the devicetree.
		 */
		if (!int_gpio_cb[i].handler) {
			/*
			 * Initialise and add the callback.
			 */
			gpio_init_callback(&int_gpio_cb[i],
					   tcpc_int_gpio_callback,
					   BIT(tcpc_config[i].int_cfg.pin));
			gpio_add_callback(tcpc_config[i].int_cfg.port,
					  &int_gpio_cb[i]);
		}
		flags = tcpc_config[i].flags & TCPC_FLAGS_ALERT_ACTIVE_HIGH ?
				GPIO_INT_EDGE_RISING :
				GPIO_INT_EDGE_FALLING;
		flags = (flags | GPIO_INT_ENABLE) & ~GPIO_INT_DISABLE;
		gpio_pin_interrupt_configure_dt(&tcpc_config[i].int_cfg, flags);
	}
}
DECLARE_HOOK(HOOK_INIT, tcpc_enable_interrupt, HOOK_PRIO_POST_I2C);

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
