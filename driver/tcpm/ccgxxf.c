/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Type-C port manager for Cypress EZ-PD CCG6DF, CCG6SF */

#include "ccgxxf.h"
#include "console.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"


#if !defined(CONFIG_USB_PD_TCPM_PPC_CCGXXF)
#error "Unsupported CCGXXF TCPC."
#endif

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/*  CCGXXF FW is designed to adapt standard TCPM driver procedures.*/
const struct tcpm_drv ccgxxf_tcpm_drv = {
	.init			= &tcpci_tcpm_init,
	.release		= &tcpci_tcpm_release,
	.get_cc			= &tcpci_tcpm_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level	= &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value	= &tcpci_tcpm_select_rp_value,
	.set_cc			= &tcpci_tcpm_set_cc,
	.set_polarity		= &tcpci_tcpm_set_polarity,
	.set_vconn		= &tcpci_tcpm_set_vconn,
	.set_msg_header		= &tcpci_tcpm_set_msg_header,
	.set_rx_enable		= &tcpci_tcpm_set_rx_enable,
	.get_message_raw	= &tcpci_tcpm_get_message_raw,
	.transmit		= &tcpci_tcpm_transmit,
	.tcpc_alert		= &tcpci_tcpc_alert,
#ifdef CONFIG_USB_PD_DECODE_SOP
	.sop_prime_enable	= &tcpci_tcpm_sop_prime_enable,
#endif
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus	= &tcpci_tcpc_discharge_vbus,
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle		= &tcpci_tcpc_drp_toggle,
#endif
	.get_chip_info		= &tcpci_get_chip_info,
#ifdef CONFIG_USBC_PPC
	.set_snk_ctrl		= &tcpci_tcpm_set_snk_ctrl,
	.set_src_ctrl		= &tcpci_tcpm_set_src_ctrl,
#endif
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode	= &tcpci_enter_low_power_mode,
#endif
	.set_bist_test_mode	= &tcpci_set_bist_test_mode,
	.tcpc_enable_auto_discharge_disconnect = &tcpci_tcpc_enable_auto_discharge_disconnect,
#ifdef CONFIG_CMD_TCPC_DUMP
	.dump_registers		= &tcpc_dump_std_registers,
#endif
};


const struct ppc_drv ccgxxf_ppc_drv = {
	.vbus_sink_enable = &tcpci_tcpm_set_snk_ctrl,
	.vbus_source_enable = &tcpci_tcpm_set_src_ctrl,
};

int ccgxxf_gpio_set(int port, enum ccgxxf_gpios gpio, enum ccgxxf_gpios_state gpio_value)
{
	return tcpc_update8(port, TCPC_REG_VENDOR_GPIO_CTRL, gpio, gpio_value);
}

/* Get the GPIO */
int ccgxxf_gpio_get(int port, enum ccgxxf_gpios gpio, enum ccgxxf_gpios_state *gpio_value)
{
	int rv;
	int read_v;
	rv = tcpc_read(port, TCPC_REG_VENDOR_GPIO_CTRL, &read_v);
	*gpio_value = (read_v & gpio) ? CCG6_GPIO_SET : CCG6_GPIO_CLR;  
	return rv;
};


/*
 * TODO: F/W upgrade module. move it to depthcharge.
 * Caution: Do not upload binary if it's propritery
 */
const uint8_t fw[] = {
	/* Add binary hex data */
};

static int ccgxxf_fw_update(int port)
{
	/* Add I2C based logic here */
	CPRINTS("ccgxxf fw update complete on port %d", port);

	return EC_SUCCESS;
}

static int console_command_ccgxxf_fw_update(int argc, char **argv)
{
	char *e;
	int port;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	/* get port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || !board_is_usb_pd_port_present(port) ||
		tcpc_config[port].drv != &ccgxxf_tcpm_drv)
		return EC_ERROR_PARAM1;

	return ccgxxf_fw_update(port);
}
DECLARE_CONSOLE_COMMAND(ccg_fw, console_command_ccgxxf_fw_update,
			"port", "Update ccgxxf firmware");
