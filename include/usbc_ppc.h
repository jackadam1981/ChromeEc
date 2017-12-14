/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USBC_PPC_H
#define __CROS_EC_USBC_PPC_H

#include "common.h"
#include "gpio.h"
#include "usb_pd_tcpm.h"

/* Common APIs for USB Type-C Power Path Controllers (PPC) */

/* Number of times a port may overcurrent before we latch off the port. */
#define PPC_OC_CNT_THRESH 3

struct ppc_drv {
	/**
	 * Initialize the PPC.
	 *
	 * @param port: The Type-C port number.
	 * @return EC_SUCCESS when init was successful, error otherwise.
	 */
	int (*init)(int port);

	/**
	 * Is the port sourcing Vbus?
	 *
	 * @param port: The Type-C port number.
	 * @return 1 if sourcing Vbus, 0 if not.
	 */
	int (*is_sourcing_vbus)(int port);

	/**
	 * Turn on/off the charge path FET, such that current flows into the
	 * system.
	 *
	 * @param port: The Type-C port number.
	 * @param enable: 1: Turn on the FET, 0: turn off the FET.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*vbus_sink_enable)(int port, int enable);

	/**
	 * Turn on/off the source path FET, such that current flows from the
	 * system.
	 *
	 * @param port: The Type-C port number.
	 * @param enable: 1: Turn on the FET, 0: turn off the FET.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*vbus_source_enable)(int port, int enable);

	/**
	 * Set the Vbus source path current limit
	 *
	 * @param port: The Type-C port number.
	 * @param rp: The Rp value which to approximately set the current limit.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*set_vbus_source_current_limit)(int port, enum tcpc_rp_value rp);

#ifdef CONFIG_CMD_PPC_DUMP
	/**
	 * Perform a register dump of the PPC.
	 *
	 * @param port: The Type-C port number.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*reg_dump)(int port);
#endif /* defined(CONFIG_CMD_PPC_DUMP) */

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
	/*
	 * TODO(aaboagye): In order for VBUS detection to work properly for our
	 * system, we need to enable VBUS interrupts and send the appropriate
	 * notifications.
	 */

	/**
	 * Determine if VBUS is present or not.
	 *
	 * @param port: The Type-C port number.
	 * @param vbus_present: 1: VBUS is present. 0: VBUS is not present.
	 * @return EC_SUCCESS if able to determine VBUS status, otherwise an
	 *         error.
	 */
	int (*is_vbus_present)(int port, int *vbus_present);
#endif /* defined(CONFIG_USB_PD_VBUS_DETECT_PPC) */

	/**
	 * The interrupt service routine for the PPC.
	 *
	 * NOTE: This is called in PDCMD context.
	 * @param port: The Type-C port number.
	 */
	void (*isr)(int port);
};

struct ppc_config_t {
	int i2c_port;
	int i2c_addr;
	const struct ppc_drv *drv;
#ifdef CONFIG_USBC_PPC_SHARED_IRQ
	enum gpio_signal int_pin;
#endif /* defined(CONFIG_USBC_PPC_SHARED_IRQ) */
};

extern const struct ppc_config_t ppc_chips[];
extern const unsigned int ppc_cnt;

/**
 * Increment the overcurrent event counter.
 *
 * @param port: The Type-C port that has overcurrented.
 * @return EC_SUCCESS on success, EC_ERROR_INVAL if non-existent port.
 */
int ppc_add_oc_event(int port);

/**
 * Clear the overcurrent event counter.
 *
 * @param port: The Type-C port's counter to clear.
 * @return EC_SUCCESS on success, EC_ERROR_INVAL if non-existent port.
 */
int ppc_clear_oc_event_counter(int port);

/**
 * Handle any pending interrupts.
 */
void ppc_handle_pending_irqs(void);

/**
 * Determine if VBUS is present or not.
 *
 * @param port: The Type-C port number.
 * @param vbus_present: 1: VBUS is present. 0: VBUS is not present.
 * @return EC_SUCCESS if able to determine VBUS status, otherwise an
 *         error.
 */
int ppc_is_vbus_present(int port, int *vbus_present);

/**
 * Is the port sourcing Vbus?
 *
 * @param port: The Type-C port number.
 * @return 1 if sourcing Vbus, 0 if not.
 */
int ppc_is_sourcing_vbus(int port);

/**
 * Re-enable all PPC interrupts.
 *
 * This is used if the PPC shares its interrupt line with the TCPC
 * (CONFIG_USBC_PPC_SHARED_IRQ).
 */
void ppc_reenable_irqs(void);

/**
 * Indicate that there's a pending PPC IRQ for this port.
 *
 * @param port: The Type-C port number.
 */
void ppc_set_pending_irq(int port);

/**
 * Set the Vbus source path current limit
 *
 * @param port: The Type-C port number.
 * @param rp: The Rp value which to approximately set the current limit.
 * @return EC_SUCCESS on success, error otherwise.
 */
int ppc_set_vbus_source_current_limit(int port, enum tcpc_rp_value rp);

/**
 * Turn on/off the charge path FET, such that current flows into the
 * system.
 *
 * @param port: The Type-C port number.
 * @param enable: 1: Turn on the FET, 0: turn off the FET.
 * @return EC_SUCCESS on success, error otherwise.
 */
int ppc_vbus_sink_enable(int port, int enable);

/**
 * Turn on/off the source path FET, such that current flows from the
 * system.
 *
 * @param port: The Type-C port number.
 * @param enable: 1: Turn on the FET, 0: turn off the FET.
 * @return EC_SUCCESS on success, error otherwise.
 */
int ppc_vbus_source_enable(int port, int enable);

/**
 * Board specific callback when a port overcurrents.
 *
 * @param port: The Type-C port which overcurrented.
 */
void board_overcurrent_event(int port);

#endif /* !defined(__CROS_EC_USBC_PPC_H) */
