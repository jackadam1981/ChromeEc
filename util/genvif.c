/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <stdint.h>

#include "config.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "genvif.h"

#define VIF_SPEC "Revision 0x54, Version 1.0"
#define PD_SPEC_REV "1"
#define VENDOR_NAME "Google"

enum dtype {SNK = 0, SRC = 3, DRP = 4};

const char *out;
const char *board;
const char *vif_producer;

volatile const uint32_t vdo_idh __attribute__((weak)) = 0;

char *yes_no(int val)
{
	return val ? "YES" : "NO";
}

static int is_src(void)
{
	return pd_src_pdo_cnt;
}

static int is_snk(void)
{
#ifdef CONFIG_USB_PD_DUAL_ROLE
	return pd_snk_pdo_cnt;
#else
	return 0;
#endif
}

static int is_extpwr(void)
{
	if (is_src())
		return !!(pd_src_pdo[0] & PDO_FIXED_EXTERNAL);
	else
		return 0;
}

static int is_drp(void)
{
	if (is_src())
		return !!(pd_src_pdo[0] & PDO_FIXED_DUAL_ROLE);
	else
		return 0;
}

static int get_vif_name(enum dtype type, char *name)
{
	char *extpwr = "_extpwr";

	if (name == NULL)
		return 1;

	if (!is_extpwr())
		extpwr = "";

	switch (type) {
	case DRP:
		if (is_drp()) {
			sprintf(name, "%s/%s_drp%s_vif.txt",
						out, board, extpwr);
			return 1;
		}
		break;

	case SRC:
		if (is_src()) {
			sprintf(name, "%s/%s_src%s_vif.txt",
						out, board, extpwr);
			return 1;
		}
		break;

	case SNK:
		if (is_snk()) {
			sprintf(name, "%s/%s_snk%s_vif.txt",
						out, board, extpwr);
			return 1;
		}
	}

	return 0;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
static char *giveback(void)
{
#ifdef CONFIG_USB_PD_GIVE_BACK
	return "YES";
#else
	return "NO";
#endif
}
#endif

static char *is_comms_cap(void)
{
	if (is_src())
		return yes_no(pd_src_pdo[0] & PDO_FIXED_COMM_CAP);
	else
		return "NO";
}

static char *dr_swap_to_ufp_supported(void)
{
	if (pd_src_pdo[0] & PDO_FIXED_DATA_SWAP)
		return yes_no(pd_check_data_swap(0, PD_ROLE_DFP));

	return "NO";
}

static char *dr_swap_to_dfp_supported(void)
{
	if (pd_src_pdo[0] & PDO_FIXED_DATA_SWAP)
		return yes_no(pd_check_data_swap(0, PD_ROLE_UFP));

	return "NO";
}

static char *vconn_swap(void)
{
#ifdef CONFIG_USBC_VCONN_SWAP
	return "YES";
#else
	return "NO";
#endif
}

static char *try_src(void)
{
#ifdef CONFIG_USB_PD_TRY_SRC
	return "YES";
#else
	return "NO";
#endif
}

static char *can_act_as_host(void)
{
#ifdef CONFIG_VIF_TYPE_C_CAN_ACT_AS_HOST
	return "YES";
#else
	return "NO";
#endif
}

static char *can_act_as_device(void)
{
#ifdef CONFIG_USB
	return "YES";
#else
	return "NO";
#endif
}

static char *supports_vconn_powered_accessory(void)
{
#ifdef CONFIG_USB_PD_TCPM_FUSB302
	return "NO";
#else
	return "YES";
#endif
}

static char *captive_cable(void)
{
#ifdef CONFIG_VIF_CAPTIVE_CABLE
	return "YES";
#else
	return "NO";
#endif
}

static char *sources_vconn(void)
{
#ifdef CONFIG_USBC_VCONN
	return "YES";
#else
	return "NO";
#endif
}

static char *battery_powered(void)
{
#if defined(CONFIG_BATTERY_BQ20Z453) || defined(CONFIG_BATTERY_BQ27541) || \
	defined(CONFIG_BATTERY_BQ27621) || defined(CONFIG_BATTERY_RYU) || \
	defined(CONFIG_BATTERY_SAMUS) || defined(CONFIG_BATTERY_SMART)
	return "YES";
#else
	return "NO";
#endif
}

static uint32_t product_type(void)
{
	return PD_IDH_PTYPE(vdo_idh);
}

static uint32_t pid_sop(void)
{
#ifdef CONFIG_USB_PID
	return CONFIG_USB_PID;
#else
	return 0;
#endif
}

static uint32_t rp_value(void)
{
#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	return CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT;
#else
	return 0;
#endif
}

static uint32_t write_pdo_to_vif(FILE *vif, uint32_t pdo,
				enum dtype type, uint32_t pnum)
{
	uint32_t power;

	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_FIXED) {
		uint32_t current = pdo & 0x3ff;
		uint32_t voltage = (pdo >> 10) & 0x3ff;

		power = ((current * 10) * (voltage * 50)) / 1000;

		fprintf(vif, "%s_PDO_Supply_Type%d: 0\n\r",
					(type == SRC) ? "Src" : "Snk", pnum);
		if (type == SRC)
			fprintf(vif, "Src_PDO_Peak_Current%d: 0\n\r", pnum);
		fprintf(vif, "%s_PDO_Voltage%d: %d\n\r",
				(type == SRC) ? "Src" : "Snk", pnum, voltage);
		if (type == SRC)
			fprintf(vif, "Src_PDO_Max_Current%d: %d\n\r",
					pnum, current);
		else
			fprintf(vif, "Snk_PDO_Op_Current%d: %d\n\r",
					pnum, current);
	} else if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
		uint32_t max_voltage = (pdo >> 20) & 0x3ff;
		uint32_t min_voltage = (pdo >> 10) & 0x3ff;

		power = pdo & 0x3ff;

		fprintf(vif, "%s_PDO_Supply_Type%d: 1\n\r",
				(type == SRC) ? "Src" : "Snk", pnum);
		fprintf(vif, "%s_PDO_Min_Voltage%d: %d\n\r",
			(type == SRC) ? "Src" : "Snk", pnum, min_voltage);
		fprintf(vif, "%s_PDO_Max_Voltage%d: %d\n\r",
			(type == SRC) ? "Src" : "Snk", pnum, max_voltage);
		if (type == SRC)
			fprintf(vif, "Src_PDO_Max_Power%d: %d\n\r",
						pnum, power);
		else
			fprintf(vif, "Snk_PDO_Op_Power%d: %d\n\r",
						pnum, power);
	} else if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_VARIABLE) {
		uint32_t max_voltage = (pdo >> 20) & 0x3ff;
		uint32_t min_voltage = (pdo >> 10) & 0x3ff;
		uint32_t current = pdo & 0x3ff;

		power = ((current * 10) * (max_voltage * 50)) / 1000;

		fprintf(vif, "%s_PDO_Supply_Type%d: 2\n\r",
				(type == SRC) ? "Src" : "Snk", pnum);
		if (type == SRC)
			fprintf(vif, "Src_PDO_Peak_Current%d: 0\n\r", pnum);
		fprintf(vif, "%s_PDO_Min_Voltage%d: %d\n\r",
			(type == SRC) ? "Src" : "Snk", pnum, min_voltage);
		fprintf(vif, "%s_PDO_Max_Voltage%d: %d\n\r",
			(type == SRC) ? "Src" : "Snk", pnum, max_voltage);
		if (type == SRC)
			fprintf(vif, "Src_PDO_Max_Current%d: %d\n\r",
						pnum, current);
		else
			fprintf(vif, "Snk_PDO_Op_Current%d: %d\n\r",
						pnum, current);
	}

	return power;
}

/**
 * Carrriage and line feed, '\n\r', is needed because the file is processed
 * on a Windows machine.
 */
static int gen_vif(enum dtype type, char *name)
{
	FILE *vif;

	/* Create VIF */
	vif = fopen(name, "w+");
	if (vif == NULL)
		return 1;

	/* Write VIF Header */
	fprintf(vif, "$VIF_Specification: \"%s\"\n\r", VIF_SPEC);
	fprintf(vif, "$VIF_Producer: \"%s\"\n\r", vif_producer);
	fprintf(vif, "$Vendor_name: \"%s\"\n\r", VENDOR_NAME);
	fprintf(vif, "$Product_Name: \"%s\"\n\r", board);

	fprintf(vif, "PD_Specification_Revision: %s\n\r", PD_SPEC_REV);
	fprintf(vif, "UUT_Device_Type: %d\n\r", type);
	fprintf(vif, "USB_Comms_Capable: %s\n\r", is_comms_cap());
	fprintf(vif, "DR_Swap_To_DFP_Supported: %s\n\r",
				dr_swap_to_dfp_supported());
	fprintf(vif, "DR_Swap_To_UFP_Supported: %s\n\r",
				dr_swap_to_ufp_supported());
	fprintf(vif, "Externally_Powered: %s\n\r", yes_no(is_extpwr()));
	fprintf(vif, "VCONN_Swap_To_On_Supported: %s\n\r", vconn_swap());
	fprintf(vif, "VCONN_Swap_To_Off_Supported: %s\n\r", vconn_swap());
	fprintf(vif, "Responds_To_Discov_SOP: YES\n\r");
	fprintf(vif, "Attempts_Discov_SOP: NO\n\r");
	fprintf(vif, "SOP_Capable: YES\n\r");
	fprintf(vif, "SOP_P_Capable: NO\n\r");
	fprintf(vif, "SOP_PP_Capable: NO\n\r");
	fprintf(vif, "SOP_P_Debug_Capable: NO\n\r");
	fprintf(vif, "SOP_PP_Debug_Capable: NO\n\r");

	/* Write Source Fields */
	if (type == DRP || type == SRC) {
		uint32_t max_power = 0;

		fprintf(vif, "USB_Suspend_May_Be_Cleared: NO\n\r");
		fprintf(vif, "Sends_Pings: NO\n\r");
		fprintf(vif, "Num_Src_PDOs: %d\n\r", pd_src_pdo_cnt);

		/* Write Source PDOs */
		{
		int i;
		uint32_t pwr;

		for (i = 0; i < pd_src_pdo_cnt; i++) {
			pwr = write_pdo_to_vif(vif, pd_src_pdo[i], SRC, i+1);
			if (pwr > max_power)
				max_power = pwr;
		}
		}

		fprintf(vif, "PD_Power_as_Source: %d\n\r", max_power);
	}

	/* Write Sink Fields */
#ifdef CONFIG_USB_PD_DUAL_ROLE
	if (type == DRP || type == SNK) {
		uint32_t max_power = 0;

		fprintf(vif, "USB_Suspend_May_Be_Cleared: NO\n\r");
		fprintf(vif, "GiveBack_May_Be_Set: %s\n\r", giveback());
		fprintf(vif, "Higher_Capability_Set: NO\n\r");
		fprintf(vif, "Num_Snk_PDOs: %d\n\r", pd_snk_pdo_cnt);

		/* Write Sink PDOs */
		{
		int i;
		uint32_t pwr;

		for (i = 0; i < pd_snk_pdo_cnt; i++) {
			pwr = write_pdo_to_vif(vif, pd_snk_pdo[i], SNK, i+1);
			if (pwr > max_power)
				max_power = pwr;
		}
		}

		fprintf(vif, "PD_Power_as_Sink: %d\n\r", max_power);
	}

	/* Write DRP Fields */
	if (type == DRP) {
		fprintf(vif, "Accepts_PR_Swap_As_Src: YES\n\r");
		fprintf(vif, "Accepts_PR_Swap_As_Snk: YES\n\r");
		fprintf(vif, "Requests_PR_Swap_As_Src: YES\n\r");
		fprintf(vif, "Requests_PR_Swap_As_Snk: YES\n\r");
	}
#endif

	/* SOP Discovery Fields */
	fprintf(vif, "Structured_VDM_Version_SOP: 0\n\r");
	fprintf(vif, "XID_SOP: 0\n\r");
	fprintf(vif, "Data_Capable_as_USB_Host_SOP: YES\n\r");
	fprintf(vif, "Data_Capable_as_USB_Device_SOP: NO\n\r");
	fprintf(vif, "Product_Type_SOP: %d\n\r", product_type());
	fprintf(vif, "Modal_Operation_Supported_SOP: YES\n\r");
	fprintf(vif, "USB_VID_SOP: 0x%04x\n\r", USB_VID_GOOGLE);
	fprintf(vif, "PID_SOP: 0x%04x\n\r", pid_sop());
	fprintf(vif, "bcdDevice_SOP: 0x0000\n\r");

	fprintf(vif, "SVID1_SOP: %04x\n\r", USB_VID_GOOGLE);
	fprintf(vif, "SVID1_num_modes_min_SOP: 1\n\r");
	fprintf(vif, "SVID1_num_modes_max_SOP: 1\n\r");
	fprintf(vif, "SVID1_num_modes_fixed_SOP: YES\n\r");
	fprintf(vif, "SVID1_mode1_enter_SOP: YES\n\r");

#ifdef USB_SID_DISPLAYPORT
	fprintf(vif, "SVID2_SOP: %04x\n\r", USB_VID_GOOGLE);
	fprintf(vif, "SVID2_num_modes_min_SOP: 2\n\r");
	fprintf(vif, "SVID2_num_modes_max_SOP: 2\n\r");
	fprintf(vif, "SVID2_num_modes_fixed_SOP: YES\n\r");
	fprintf(vif, "SVID2_mode1_enter_SOP: YES\n\r");
	fprintf(vif, "SVID2_mode2_enter_SOP: YES\n\r");

	fprintf(vif, "Num_SVIDs_min_SOP: 2\n\r");
	fprintf(vif, "Num_SVIDs_max_SOP: 2\n\r");
	fprintf(vif, "SVID_fixed_SOP: YES\n\r");
#else
	fprintf(vif, "Num_SVIDs_min_SOP: 1\n\r");
	fprintf(vif, "Num_SVIDs_max_SOP: 1\n\r");
	fprintf(vif, "SVID_fixed_SOP: YES\n\r");
#endif

	/* set Type_C_State_Machine */
	{
	int typec;

	switch (type) {
	case DRP:
		typec = 2;
		break;

	case SNK:
		typec = 1;
		break;

	default:
		typec = 0;
	}

	fprintf(vif, "Type_C_State_Machine: %d\n\r", typec);
	}

	fprintf(vif, "Type_C_Implements_Try_SRC: %s\n\r", try_src());
	fprintf(vif, "Type_C_Implements_Try_SNK: NO\n\r");
	fprintf(vif, "Rp_Value: %d\n\r", rp_value());
	fprintf(vif, "Type_C_Supports_VCONN_Powered_Accessory: %s\n\r",
		supports_vconn_powered_accessory());
	fprintf(vif, "Type_C_Is_VCONN_Powered_Accessory: NO\n\r");
	fprintf(vif, "Type_C_Can_Act_As_Host: %s\n\r", can_act_as_host());
	fprintf(vif, "Type_C_Host_Speed: 4\n\r");
	fprintf(vif, "Type_C_Can_Act_As_Device: %s\n\r", can_act_as_device());
	fprintf(vif, "Type_C_Device_Speed: 4\n\r");
	fprintf(vif, "Type_C_Power_Source: 2\n\r");
	fprintf(vif, "Type_C_BC_1_2_Support: 1\n\r");
	fprintf(vif, "Type_C_Battery_Powered: %s\n\r", battery_powered());
	fprintf(vif, "Type_C_Port_On_Hub: NO\n\r");
	fprintf(vif, "Type_C_Supports_Audio_Accessory: NO\n\r");
	fprintf(vif, "Captive_Cable: %s\n\r", captive_cable());
	fprintf(vif, "Type_C_Source_Vconn: %s\n\r", sources_vconn());

	fclose(vif);
	return 0;
}

int main(int argc, char **argv)
{
	int nopt;
	DIR *vifdir;
	char name[25];
	const char * const short_opt = "hb:o:";
	const struct option long_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "board", 1, NULL, 'b' },
		{ "out", 1, NULL, 'o' },
		{ NULL, 0, NULL, 0 }
	};

	vif_producer = argv[0];

	do {
		nopt = getopt_long(argc, argv, short_opt, long_opts, NULL);
		switch (nopt) {
		case 'h': /* -h or --help */
			printf("USAGE: %s -b <board name> -o <out directory>\n",
					vif_producer);
			return 0;

		case 'b': /* -b or --board */
			board = optarg;
			break;

		case 'o': /* -o or --out */
			out = optarg;
			break;

		case -1:
			break;

		default:
			abort();
		}
	} while (nopt != -1);

	if (out == NULL || board == NULL)
		return 1;

	/* Make sure VIF directory exists */
	vifdir = opendir(out);
	if (vifdir == NULL) {
		printf("ERROR: %s directory does not exist.\n", out);
		return 1;
	}
	closedir(vifdir);

	if (get_vif_name(DRP, name)) {
		if (gen_vif(DRP, name))
			return 1;
	} else {
		if (get_vif_name(SRC, name))
			if (gen_vif(SRC, name))
				return 1;
		if (get_vif_name(SNK, name))
			if (gen_vif(SNK, name))
				return 1;
	}

	return 0;
}
