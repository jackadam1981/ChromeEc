/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "usb_pe_sm.h"
#include "usb_tc_sm.h"
#include "usb_pd.h"
#include "util.h"

/**
 * PD3.0 Console commands
 *
 * > pd %d state
 *	To see what the PD state is, e.g. SRC/SNK, DFP/UFP, CC1/CC2, flags, etc.
 */

/*
 * This function converts an 8 character ascii string with hex digits, without
 * the 0x prefix, into a signed 32-bit number.
 */
test_export_static int hex8tou32(char *str, uint32_t *val)
{
	char *ptr = str;
	uint32_t tmp = 0;

	while (ptr && *ptr) {
		char c = *ptr++;

		if (c >= '0' && c <= '9')
			tmp = (tmp << 4) + (c - '0');
		else if (c >= 'A' && c <= 'F')
			tmp = (tmp << 4) + (c - 'A' + 10);
		else if (c >= 'a' && c <= 'f')
			tmp = (tmp << 4) + (c - 'a' + 10);
		else
			return EC_ERROR_INVAL;
	}
	if (ptr != str + 8)
		return EC_ERROR_INVAL;
	*val = tmp;
	return EC_SUCCESS;
}

test_export_static int remote_flashing(int argc, char **argv)
{
	int port, cnt, cmd;
	uint32_t data[VDO_MAX_SIZE-1];
	char *e;
	static int flash_offset[CONFIG_USB_PD_PORT_MAX_COUNT];

	if (argc < 4 || argc > (VDO_MAX_SIZE + 4 - 1))
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[1], &e, 10);
	if (*e || port >= board_get_usb_pd_port_count())
		return EC_ERROR_PARAM2;

	cnt = 0;
	if (!strcasecmp(argv[3], "erase")) {
		cmd = VDO_CMD_FLASH_ERASE;
		flash_offset[port] = 0;
		ccprintf("ERASE ...");
	} else if (!strcasecmp(argv[3], "reboot")) {
		cmd = VDO_CMD_REBOOT;
		ccprintf("REBOOT ...");
	} else if (!strcasecmp(argv[3], "signature")) {
		cmd = VDO_CMD_ERASE_SIG;
		ccprintf("ERASE SIG ...");
	} else if (!strcasecmp(argv[3], "info")) {
		cmd = VDO_CMD_READ_INFO;
		ccprintf("INFO...");
	} else if (!strcasecmp(argv[3], "version")) {
		cmd = VDO_CMD_VERSION;
		ccprintf("VERSION...");
	} else {
		int i;

		argc -= 3;
		for (i = 0; i < argc; i++)
			if (hex8tou32(argv[i+3], data + i))
				return EC_ERROR_INVAL;
		cmd = VDO_CMD_FLASH_WRITE;
		cnt = argc;
		ccprintf("WRITE %d @%04x ...", argc * 4,
			 flash_offset[port]);
		flash_offset[port] += argc * 4;
	}

	pe_send_vdm(port, USB_VID_GOOGLE, cmd, data, cnt);

	return EC_SUCCESS;
}

test_export_static int command_pd(int argc, char **argv)
{
	int port;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	else if (IS_ENABLED(CONFIG_USB_PD_TRY_SRC) &&
				!strncasecmp(argv[1], "trysrc", 6)) {
		enum try_src_override_t ov = tc_get_try_src_override();

		if (argc >= 3) {
			ov = strtoi(argv[2], &e, 10);
			if (*e || ov > TRY_SRC_NO_OVERRIDE)
				return EC_ERROR_PARAM3;
			tc_try_src_override(ov);
		}

		if (ov == TRY_SRC_NO_OVERRIDE)
			ccprintf("Try.SRC System controlled\n");
		else
			ccprintf("Try.SRC Forced %s\n", ov ? "ON" : "OFF");

		return EC_SUCCESS;
	}

	/* command: pd <port> <subcmd> [args] */
	port = strtoi(argv[1], &e, 10);
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	if (*e || port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return EC_ERROR_PARAM2;

	if (IS_ENABLED(CONFIG_USB_PD_DUAL_ROLE)) {
		if (!strcasecmp(argv[2], "tx")) {
			pe_dpm_request(port, DPM_REQUEST_SNK_STARTUP);
		} else if (!strcasecmp(argv[2], "bist_rx")) {
			pe_dpm_request(port, DPM_REQUEST_BIST_RX);
		} else if (!strcasecmp(argv[2], "bist_tx")) {
			if (*e)
				return EC_ERROR_PARAM3;
			pe_dpm_request(port, DPM_REQUEST_BIST_TX);
		} else if (!strcasecmp(argv[2], "charger")) {
			pe_dpm_request(port, DPM_REQUEST_SRC_STARTUP);
		} else if (!strncasecmp(argv[2], "dev", 3)) {
			int max_volt;

			if (argc >= 4)
				max_volt = strtoi(argv[3], &e, 10) * 1000;
			else
				max_volt = pd_get_max_voltage();
			pd_request_source_voltage(port, max_volt);
			pe_dpm_request(port, DPM_REQUEST_NEW_POWER_LEVEL);
			ccprintf("max req: %dmV\n", max_volt);
		} else if (!strcasecmp(argv[2], "disable")) {
			pd_comm_enable(port, 0);
			ccprintf("Port C%d disable\n", port);
			return EC_SUCCESS;
		} else if (!strcasecmp(argv[2], "enable")) {
			pd_comm_enable(port, 1);
			ccprintf("Port C%d enabled\n", port);
			return EC_SUCCESS;
		} else if (!strncasecmp(argv[2], "hard", 4)) {
			pe_dpm_request(port, DPM_REQUEST_HARD_RESET_SEND);
		} else if (!strncasecmp(argv[2], "info", 4)) {
			tc_print_dev_info(port);
		} else if (!strncasecmp(argv[2], "soft", 4)) {
			pe_dpm_request(port, DPM_REQUEST_SOFT_RESET_SEND);
		} else if (!strncasecmp(argv[2], "swap", 4)) {
			if (argc < 4)
				return EC_ERROR_PARAM_COUNT;

			if (!strncasecmp(argv[3], "power", 5))
				pe_dpm_request(port, DPM_REQUEST_PR_SWAP);
			else if (!strncasecmp(argv[3], "data", 4))
				pe_dpm_request(port, DPM_REQUEST_DR_SWAP);
			else if (IS_ENABLED(CONFIG_USBC_VCONN_SWAP) &&
					!strncasecmp(argv[3], "vconn", 5))
				pe_dpm_request(port, DPM_REQUEST_VCONN_SWAP);
			else
				return EC_ERROR_PARAM3;
		} else if (!strncasecmp(argv[2], "ping", 4)) {
			pe_dpm_request(port, DPM_REQUEST_SEND_PING);
		} else if (!strncasecmp(argv[2], "vdm", 3)) {
			if (argc < 4)
				return EC_ERROR_PARAM_COUNT;

			if (!strncasecmp(argv[3], "ping", 4)) {
				uint32_t enable;

				if (argc < 5)
					return EC_ERROR_PARAM_COUNT;
				enable = strtoi(argv[4], &e, 10);
				if (*e)
					return EC_ERROR_PARAM4;
				pe_send_vdm(port, USB_VID_GOOGLE,
							VDO_CMD_PING_ENABLE,
					&enable, 1);
			} else if (!strncasecmp(argv[3], "curr", 4)) {
				pe_send_vdm(port, USB_VID_GOOGLE,
						VDO_CMD_CURRENT, NULL, 0);
			} else if (!strncasecmp(argv[3], "vers", 4)) {
				pe_send_vdm(port, USB_VID_GOOGLE,
						VDO_CMD_VERSION, NULL, 0);
			} else {
				return EC_ERROR_PARAM_COUNT;
			}
		} else if (IS_ENABLED(CONFIG_CMD_PD_FLASH) &&
				!strncasecmp(argv[2], "flash", 4)) {
			return remote_flashing(argc, argv);
		} else if (!strcasecmp(argv[2], "dualrole")) {
			if (argc < 4) {
				ccprintf("dual-role toggling: ");
				switch (pd_get_dual_role(port)) {
				case PD_DRP_TOGGLE_ON:
					ccprintf("on\n");
					break;
				case PD_DRP_TOGGLE_OFF:
					ccprintf("off\n");
					break;
				case PD_DRP_FREEZE:
					ccprintf("freeze\n");
					break;
				case PD_DRP_FORCE_SINK:
					ccprintf("force sink\n");
					break;
				case PD_DRP_FORCE_SOURCE:
					ccprintf("force source\n");
					break;
				}
			} else {
				if (!strcasecmp(argv[3], "on"))
					pd_set_dual_role(port,
							PD_DRP_TOGGLE_ON);
				else if (!strcasecmp(argv[3], "off"))
					pd_set_dual_role(port,
							PD_DRP_TOGGLE_OFF);
				else if (!strcasecmp(argv[3], "freeze"))
					pd_set_dual_role(port, PD_DRP_FREEZE);
				else if (!strcasecmp(argv[3], "sink"))
					pd_set_dual_role(port,
							PD_DRP_FORCE_SINK);
				else if (!strcasecmp(argv[3], "source"))
					pd_set_dual_role(port,
							PD_DRP_FORCE_SOURCE);
				else
					return EC_ERROR_PARAM4;
			}
			return EC_SUCCESS;
		}
	}

	if (!strncasecmp(argv[2], "state", 5)) {
		ccprintf("Port C%d CC%d, %s - Role: %s-%s",
		port, pd_get_polarity(port) + 1,
		pd_comm_is_enabled(port) ? "Enable" : "Disable",
		pd_get_power_role(port) ==
					PD_ROLE_SOURCE ? "SRC" : "SNK",
		pd_get_data_role(port) == PD_ROLE_DFP ? "DFP" : "UFP");

		if (IS_ENABLED(CONFIG_USBC_VCONN))
			ccprintf("%s ", tc_is_vconn_src(port) ? "-VC" : "");

		ccprintf("TC State: %s, Flags: 0x%04x\n",
			tc_get_current_state(port),
			tc_get_flags(port));
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pd, command_pd,
#ifdef CONFIG_USB_PD_TRY_SRC
	"trysrc [0|1|2]"
#endif
#ifdef CONFIG_CMD_PD_DEV_DUMP_INFO
	"|rwhashtable"
#endif
	"\n\t<port> state"
#ifdef CONFIG_USB_PD_DUAL_ROLE
	"|tx|bist_rx|bist_tx|charger|dev"
	"\n\t<port> disable|enable|soft|info|hard|ping"
	"\n\t<port> dualrole [on|off|freeze|sink|source]"
	"\n\t<port> swap [power|data|vconn]"
	"\n\t<port> vdm [ping|curr|vers]"
#ifdef CONFIG_CMD_PD_FLASH
	"\n\t<port> flash [erase|reboot|signature|info|version]"
#endif /* CONFIG_CMD_PD_FLASH */
#endif /* CONFIG_USB_PD_DUAL_ROLE */
	,
	"USB PD");
