/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB charging control module for Chrome EC */

#ifndef __CROS_EC_USB_CHARGE_H
#define __CROS_EC_USB_CHARGE_H

#include "common.h"

/* USB charger voltage */
#define USB_CHARGER_VOLTAGE_MV  5000
/* USB charger minimum current */
#define USB_CHARGER_MIN_CURR_MA 500

enum usb_charge_mode {
	/* Disable USB port. */
	USB_CHARGE_MODE_DISABLED,
	/* Set USB port to Standard Downstream Port, USB 2.0 mode. */
	USB_CHARGE_MODE_SDP2,
	/* Set USB port to Charging Downstream Port, BC 1.2. */
	USB_CHARGE_MODE_CDP,
	/* Set USB port to Dedicated Charging Port, BC 1.2. */
	USB_CHARGE_MODE_DCP_SHORT,
	/* Enable USB port (for dumb ports). */
	USB_CHARGE_MODE_ENABLED,

	USB_CHARGE_MODE_COUNT
};

struct usb_ch_drv {
	/**
	 * Initialize chip
	 *
	 * @param port Type-C port number
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*init)(int port);

	/**
	 * Reset the chip
	 *
	 * @param port Type-C port number
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*reset)(int port);

	/**
	 * Get attached device type (DCP, CDP, SDP, Propritery, other)
	 *
	 * @param port Type-C port number
	 *
	 * @return attached device type
	 */
	int (*get_dev_type)(int port);

	/**
	 * Get attached BC1.2 charger type (DCP, CDP, SDP, Propritery, other)
	 *
	 * @param port Type-C port number
	 * @param charger_status Charger status
	 * @param device_type Charger device_type
	 *
	 * @return attached BC1.2 charger type
	 */
	int (*get_chg_type)(int port, int charger_status, int device_type);

	/**
	 * Get attached charger status
	 *
	 * @param port Type-C port number
	 *
	 * @return Attached charger status
	 */
	int (*get_chg_status)(int port);

	/**
	 * Get charger any detection status
	 *
	 * @param port Type-C port number
	 *
	 * @return 1 or 0
	 */
	int (*get_chg_any_det)(int port, int charger_status);

	/**
	 * Enable interrupts
	 *
	 * @param port Type-C port number
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*enable_intr)(int port);

	/**
	 * Disable interrupts
	 *
	 * @param port Type-C port number
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*disable_intr)(int port);

	/**
	 * Get interrupts
	 *
	 * @param port Type-C port number
	 * @return Interrupts if any
	 */
	int (*get_intr)(int port);

	/**
	 * Set interrupts mask
	 *
	 * @param port Type-C port number
	 * @param mask Mask value
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*set_intr_mask)(int port, int mask);

	/**
	 * Attach available interrupt masks
	 *
	 * @param port Type-C port number
	 *
	 * @return available interrupt masks
	 */
	int (*attach_mask)(int port);

	/**
	 * Set D+/D-/Vbus switches to open or closed/auto-control
	 *
	 * @param port Type-C port number
	 * @param val  Value to be set
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*set_switches)(int port, int val);

	/**
	 * Set switch configuration to manual
	 *
	 * @param port Type-C port number
	 * @param val  Value to be set
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*set_switch_manual)(int port, int val);

	/**
	 * Set bits to enable pins in manual switch register
	 *
	 * @param port Type-C port number
	 * @param val  Value to be set
	 *
	 * @return EC_SUCCESS or error
	 */
	int (*set_pins)(int port, int val);

	/**
	 * Get charger current limit based on device type and charger status
	 *
	 * @param port Type-C port number
	 * @param device_type
	 * @param charger_status
	 *
	 * @return current limit
	 */
	int (*get_ilim)(int port, int device_type, int charger_status);
};

/* Define configuration of USB charger port */
struct usb_charger {
	/* USB charger driver */
	const struct usb_ch_drv *driver;
};

/**
 * Set USB charge mode for the port.
 *
 * @param usb_port_id	Port to set.
 * @param mode		New mode for port.
 * @return EC_SUCCESS, or non-zero if error.
 */
int usb_charge_set_mode(int usb_port_id, enum usb_charge_mode mode);

/**
 * Return a bitmask of which USB ports are enabled.
 *
 * If bit (1 << i) is set, port <i> is enabled.  If it is clear, port <i> is
 * in USB_CHARGE_MODE_DISABLED.
 */
int usb_charge_ports_enabled(void);

/* Events handled by the USB_CHG task */
#define USB_CHG_EVENT_BC12 TASK_EVENT_CUSTOM(1)
#define USB_CHG_EVENT_VBUS TASK_EVENT_CUSTOM(2)
#define USB_CHG_EVENT_INTR TASK_EVENT_CUSTOM(4)

/**
 * Returns true if the passed port is a power source.
 *
 * @param port  Port number.
 * @return      True if port is sourcing vbus.
 */
int usb_charger_port_is_sourcing_vbus(int port);

enum usb_switch {
	USB_SWITCH_CONNECT,
	USB_SWITCH_DISCONNECT,
	USB_SWITCH_RESTORE,
};

/**
 * Configure USB data switches on type-C port.
 *
 * @param port port number.
 * @param setting new switch setting to configure.
 */
void usb_charger_set_switches(int port, enum usb_switch setting);

/**
 * Notify USB_CHG task that VBUS level has changed.
 *
 * @param port port number.
 * @param vbus_level new VBUS level
 */
void usb_charger_vbus_change(int port, int vbus_level);

extern struct usb_charger usb_chargers[];

#endif  /* __CROS_EC_USB_CHARGE_H */
