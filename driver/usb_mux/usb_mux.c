/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB mux high-level driver. */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static int enable_debug_prints;

/*
 * Flags will reset to 0 after sysjump; This works for current flags as LPM will
 * get reset in the init method which is called during PD task startup.
 */
static uint8_t flags[CONFIG_USB_PD_PORT_MAX_COUNT];

#define USB_MUX_FLAG_IN_LPM BIT(0) /* Device is in low power mode. */

<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
=======
enum mux_config_type {
	USB_MUX_INIT,
	USB_MUX_LOW_POWER,
	USB_MUX_SET_MODE,
	USB_MUX_GET_MODE,
};

/* Configure the MUX */
static int configure_mux(int port,
			 enum mux_config_type config,
			 mux_state_t *mux_state)
{
	int rv = EC_SUCCESS;
	const struct usb_mux *mux_ptr;

	if (config == USB_MUX_SET_MODE ||
	    config == USB_MUX_GET_MODE) {
		if (mux_state == NULL)
			return EC_ERROR_INVAL;

		if (config == USB_MUX_GET_MODE)
			*mux_state = USB_PD_MUX_NONE;
	}

	/*
	 * a MUX for a particular port can be a linked list chain of
	 * MUXes.  So when we change one, we traverse the whole list
	 * to make sure they are all updated appropriately.
	 */
	for (mux_ptr = &usb_muxes[port];
	     rv == EC_SUCCESS && mux_ptr != NULL;
	     mux_ptr = mux_ptr->next_mux) {
		mux_state_t lcl_state;
		const struct usb_mux_driver *drv = mux_ptr->driver;

		switch (config) {
		case USB_MUX_INIT:
			if (drv && drv->init) {
				rv = drv->init(mux_ptr);
				if (rv)
					break;
			}

			/* Apply board specific initialization */
			if (mux_ptr->board_init)
				rv = mux_ptr->board_init(mux_ptr);

			break;

		case USB_MUX_LOW_POWER:
			if (drv && drv->enter_low_power_mode)
				rv = drv->enter_low_power_mode(mux_ptr);

			break;

		case USB_MUX_SET_MODE:
			lcl_state = *mux_state;

			if (mux_ptr->flags & USB_MUX_FLAG_SET_WITHOUT_FLIP)
				lcl_state &= ~USB_PD_MUX_POLARITY_INVERTED;

			if (drv && drv->set) {
				rv = drv->set(mux_ptr, lcl_state);
				if (rv)
					break;
			}

			/* Apply board specific setting */
			if (mux_ptr->board_set)
				rv = mux_ptr->board_set(mux_ptr, lcl_state);

			break;

		case USB_MUX_GET_MODE:
			/*
			 * This is doing a GET_CC on all of the MUXes in the
			 * chain and ORing them together. This will make sure
			 * if one of the MUX values has FLIP turned off that
			 * we will end up with the correct value in the end.
			 */
			if (drv && drv->get) {
				rv = drv->get(mux_ptr, &lcl_state);
				if (rv)
					break;
				*mux_state |= lcl_state;
			}
			break;
		}
	}

	if (rv)
		CPRINTS("mux config:%d, port:%d, rv:%d",
			config, port, rv);

	return rv;
}
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)

static void enter_low_power_mode(int port)
{
<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
	const struct usb_mux *mux = &usb_muxes[port];
	int res;

=======
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)
	/*
	 * Set LPM flag regardless of method presence or method failure. We
	 * want know know that we tried to put the device in low power mode
	 * so we can re-initialize the device on the next access.
	 */
	flags[port] |= USB_MUX_FLAG_IN_LPM;

	/* Apply any low power customization if present */
<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
	if (mux->driver->enter_low_power_mode) {
		res = mux->driver->enter_low_power_mode(port);

		if (res)
			CPRINTS("Err: enter_low_power_mode mux port(%d): %d",
				port, res);
	}
=======
	configure_mux(port, USB_MUX_LOW_POWER, NULL);
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)
}

static inline void exit_low_power_mode(int port)
{
	/* If we are in low power, initialize device (which clears LPM flag) */
	if (flags[port] & USB_MUX_FLAG_IN_LPM)
		usb_mux_init(port);
}

void usb_mux_init(int port)
{
<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
	const struct usb_mux *mux = &usb_muxes[port];
	int res;

=======
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)
	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_MAX_COUNT);

<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
	res = mux->driver->init(port);
	if (res) {
		CPRINTS("Err: init mux port(%d): %d", port, res);
		return;
	}
=======
	configure_mux(port, USB_MUX_INIT, NULL);
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)

	/* Device is always out of LPM after initialization. */
	flags[port] &= ~USB_MUX_FLAG_IN_LPM;

	/* Apply board specific initialization */
	if (mux->board_init) {
		res = mux->board_init(port);

		if (res)
			CPRINTS("Err: board_init mux port(%d): %d", port, res);
	}
}

/*
 * TODO(crbug.com/505480): Setting muxes often involves I2C transcations,
 * which can block. Consider implementing an asynchronous task.
 */
void usb_mux_set(int port, enum typec_mux mux_mode,
		 enum usb_switch usb_mode, int polarity)
{
	const struct usb_mux *mux = &usb_muxes[port];
	int res;
	mux_state_t mux_state;
	const int should_enter_low_power_mode =
		mux_mode == TYPEC_MUX_NONE && usb_mode == USB_SWITCH_DISCONNECT;

#ifdef CONFIG_USB_CHARGER
	/* Configure USB2.0 */
	usb_charger_set_switches(port, usb_mode);
#endif

	/*
	 * Don't wake device up just to put it back to sleep. Low power mode
	 * flag is only set if the mux set() operation succeeded previously for
	 * the same disconnected state.
	 */
	if (should_enter_low_power_mode && (flags[port] & USB_MUX_FLAG_IN_LPM))
		return;

	exit_low_power_mode(port);

	/* Configure superspeed lanes */
	mux_state = polarity ? mux_mode | MUX_POLARITY_INVERTED : mux_mode;
	res = mux->driver->set(port, mux_state);
	if (res) {
		CPRINTS("Err: set mux port(%d): %d", port, res);
		return;
	}

	if (enable_debug_prints)
		CPRINTS(
		     "usb/dp mux: port(%d) typec_mux(%d) usb2(%d) polarity(%d)",
		     port, mux_mode, usb_mode, polarity);

	/*
	 * If we are completely disconnecting the mux, then we should put it in
	 * its lowest power state.
	 */
	if (should_enter_low_power_mode)
		enter_low_power_mode(port);
}

int usb_mux_get(int port, const char **dp_str, const char **usb_str)
{
	const struct usb_mux *mux = &usb_muxes[port];
	int res;
	mux_state_t mux_state;
	const char *dp, *usb;

	exit_low_power_mode(port);

	res = mux->driver->get(port, &mux_state);
	if (res) {
		CPRINTS("Err: get mux port(%d): %d", port, res);
		return 0;
	}

	dp = mux_state & MUX_POLARITY_INVERTED ? "DP2" : "DP1";
	usb = mux_state & MUX_POLARITY_INVERTED ? "USB2" : "USB1";

	*dp_str = mux_state & MUX_DP_ENABLED ? dp : NULL;
	*usb_str = mux_state & MUX_USB_ENABLED ? usb : NULL;

	return *dp_str || *usb_str;
}

void usb_mux_flip(int port)
{
	const struct usb_mux *mux = &usb_muxes[port];
	int res;
	mux_state_t mux_state;

	exit_low_power_mode(port);

	res = mux->driver->get(port, &mux_state);
	if (res) {
		CPRINTS("Err: get mux port(%d): %d", port, res);
		return;
<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
=======

	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		mux_state &= ~USB_PD_MUX_POLARITY_INVERTED;
	else
		mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	configure_mux(port, USB_MUX_SET_MODE, &mux_state);
}

void usb_mux_hpd_update(int port, int hpd_lvl, int hpd_irq)
{
	mux_state_t mux_state;
	const struct usb_mux *mux_ptr = &usb_muxes[port];

	for (; mux_ptr; mux_ptr = mux_ptr->next_mux)
		if (mux_ptr->hpd_update)
			mux_ptr->hpd_update(mux_ptr, hpd_lvl, hpd_irq);

	if (!configure_mux(port, USB_MUX_GET_MODE, &mux_state)) {
		mux_state |= (hpd_lvl ? USB_PD_MUX_HPD_LVL : 0) |
			     (hpd_irq ? USB_PD_MUX_HPD_IRQ : 0);
		configure_mux(port, USB_MUX_SET_MODE, &mux_state);
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)
	}

	if (mux_state & MUX_POLARITY_INVERTED)
		mux_state &= ~MUX_POLARITY_INVERTED;
	else
		mux_state |= MUX_POLARITY_INVERTED;

	res = mux->driver->set(port, mux_state);
	if (res)
		CPRINTS("Err: set mux port(%d): %d", port, res);
}

#ifdef CONFIG_CMD_TYPEC
static int command_typec(int argc, char **argv)
{
	const char * const mux_name[] = {"none", "usb", "dp", "dock"};
	char *e;
	int port;
	enum typec_mux mux = TYPEC_MUX_NONE;
	int i;

	if (argc == 2 && !strcasecmp(argv[1], "debug")) {
		enable_debug_prints = 1;
		return EC_SUCCESS;
	}

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[1], &e, 10);
	if (*e || port >= board_get_usb_pd_port_count())
		return EC_ERROR_PARAM1;

	if (argc < 3) {
		const char *dp_str, *usb_str;
		ccprintf("Port C%d: polarity:CC%d\n",
			port, pd_get_polarity(port) + 1);
		if (usb_mux_get(port, &dp_str, &usb_str))
			ccprintf("Superspeed %s%s%s\n",
				 dp_str ? dp_str : "",
				 dp_str && usb_str ? "+" : "",
				 usb_str ? usb_str : "");
		else
			ccprintf("No Superspeed connection\n");

		return EC_SUCCESS;
	}

	for (i = 0; i < ARRAY_SIZE(mux_name); i++)
		if (!strcasecmp(argv[2], mux_name[i]))
			mux = i;
	usb_mux_set(port, mux, mux == TYPEC_MUX_NONE ?
				      USB_SWITCH_DISCONNECT :
				      USB_SWITCH_CONNECT,
			  pd_get_polarity(port));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(typec, command_typec,
			"[port|debug] [none|usb|dp|dock]",
			"Control type-C connector muxing");
#endif

static enum ec_status hc_usb_pd_mux_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_pd_mux_info *p = args->params;
	struct ec_response_usb_pd_mux_info *r = args->response;
	int port = p->port;
<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
	const struct usb_mux *mux;
=======
	const struct usb_mux *me = &usb_muxes[port];
	mux_state_t mux_state;
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)

	if (port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	mux = &usb_muxes[port];
	if (mux->driver->get(port, &r->flags) != EC_SUCCESS)
		return EC_RES_ERROR;

#ifdef CONFIG_USB_MUX_VIRTUAL
	/* Clear HPD IRQ event since we're about to inform host of it. */
<<<<<<< HEAD   (f7e31b jinlon: moving buttons and switches to use MKBP)
	if ((r->flags & USB_PD_MUX_HPD_IRQ) &&
	    mux->hpd_update == &virtual_hpd_update)
		mux->hpd_update(port, r->flags & USB_PD_MUX_HPD_LVL, 0);
#endif
=======
	if (IS_ENABLED(CONFIG_USB_MUX_VIRTUAL) &&
	    (r->flags & USB_PD_MUX_HPD_IRQ) &&
	    (me->hpd_update == &virtual_hpd_update)) {
		usb_mux_hpd_update(port, r->flags & USB_PD_MUX_HPD_LVL, 0);
	}
>>>>>>> CHANGE (9c194f usb_mux: retimer: mux as chained mux and retimer)

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_MUX_INFO,
		     hc_usb_pd_mux_info,
		     EC_VER_MASK(0));
