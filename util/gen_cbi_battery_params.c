/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Generate cbi battery parameter from board/cbi_battery_params.c
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* Prevent from including gpio configs . */
#define __ASSEMBLER__

#include "battery_fuel_gauge.h"
#include "cbi_battery_params.h"

bool debug = false;
#define CPRINTF(format, args...) if (debug) { fprintf(fout, format, ##args); }
#define CPRINTS(format, args...) if (debug) { fprintf(stderr, format, ##args);\
						fprintf(stderr, "\n"); }

static int get_batt_params(struct board_batt_params *board_batt_info,
			int index, struct cbi_battery_info *cbi_batt_info)
{
	cbi_batt_info->header.version = CBI_BATTERY_INFO_APPLIED_VERSION;
	cbi_batt_info->header.index = index;
	memcpy(&cbi_batt_info->batt_params, board_batt_info,
		sizeof(struct board_batt_params));
	CPRINTS("get_batt_params done");
	return 0;
}

static void util_print_battery_params(struct cbi_battery_info *cbi_batt_info)
{
	const struct cbi_battery_info_header *header = &cbi_batt_info->header;
	const struct fuel_gauge_info *fg_info =
		&cbi_batt_info->batt_params.fuel_gauge;
	const struct battery_info *bt_info =
		&cbi_batt_info->batt_params.batt_info;

	CPRINTS("-----print_battery_params-----");
	CPRINTS("version:%d", header->version);
	CPRINTS("index:%d", header->index);
	CPRINTS("manuf_name:%s", fg_info->manuf_name);
	CPRINTS("device_name:%s", fg_info->device_name);
	CPRINTS("override_nil:%d", fg_info->override_nil);
	CPRINTS("wb_support:%d", fg_info->ship_mode.wb_support);
	CPRINTS("reg_addr:0x%x", fg_info->ship_mode.reg_addr);
	CPRINTS("reg_data[0]:0x%x", fg_info->ship_mode.reg_data[0]);
	CPRINTS("reg_data[1]:0x%x", fg_info->ship_mode.reg_data[1]);
	CPRINTS("sleep_supported:%d", fg_info->sleep_mode.sleep_supported);
	CPRINTS("reg_addr:0x%x", fg_info->sleep_mode.reg_addr);
	CPRINTS("reg_data:0x%x", fg_info->sleep_mode.reg_data);
	CPRINTS("mfgacc_support:%d", fg_info->fet.mfgacc_support);
	CPRINTS("mfgacc_smb_block:%d", fg_info->fet.mfgacc_smb_block);
	CPRINTS("reg_addr:0x%x", fg_info->fet.reg_addr);
	CPRINTS("reg_mask:0x%x", fg_info->fet.reg_mask);
	CPRINTS("disconnect_val:0x%x", fg_info->fet.disconnect_val);
	CPRINTS("cfet_mask:0x%x", fg_info->fet.cfet_mask);
	CPRINTS("cfet_off_val:0x%x", fg_info->fet.cfet_off_val);
	CPRINTS("voltage_max:%d", bt_info->voltage_max);
	CPRINTS("voltage_normal:%d", bt_info->voltage_normal);
	CPRINTS("voltage_min:%d", bt_info->voltage_min);
	CPRINTS("precharge_voltage:%d", bt_info->precharge_voltage);
	CPRINTS("precharge_current:%d", bt_info->precharge_current);
	CPRINTS("start_charging_min_c:%d", bt_info->start_charging_min_c);
	CPRINTS("start_charging_max_c:%d", bt_info->start_charging_max_c);
	CPRINTS("charging_min_c:%d", bt_info->charging_min_c);
	CPRINTS("charging_max_c:%d", bt_info->charging_max_c);
	CPRINTS("discharging_min_c:%d", bt_info->discharging_min_c);
	CPRINTS("discharging_max_c:%d", bt_info->discharging_max_c);
}

#define MAX_FILENAME 100

int main(int argc, char **argv)
{
	FILE *fout;
	int i, size;
	uint8_t *t;
	struct cbi_battery_info cbi_batt_info;
	struct stat st = {0};
	const char *file_pattern = "%s/v%02d_%02d_%s.bin";
	char filename[MAX_FILENAME];

	if (argc < 2 || argc > 3) {
                CPRINTS("USAGE: %s <output folder> [debug_on]", argv[0]);
                return 1;
        }
	if (argc == 3) {
		debug = (argc == 3 &&
			strncmp(argv[2], "debug_on", sizeof("debug_on")) == 0);
	}
	CPRINTS("output folder:%s", argv[1]);

	if (stat(argv[1], &st) == -1)
		mkdir(argv[1], 0700);

	size = sizeof(cbi_batt_info);
	CPRINTS("Total size:%d", size);

	for (i = 0; i < CBI_BATTERY_TYPE_COUNT; ++i) {
		memset(&cbi_batt_info, 0, size);
		memset(&filename, 0, MAX_FILENAME);

		if (get_batt_params(&cbi_board_battery_info[i], i,
			&cbi_batt_info)) {
			CPRINTS("Failed to get battery parameter");
			return 2;
		}
		sprintf(filename, file_pattern, argv[1],
			cbi_batt_info.header.version,
			cbi_batt_info.header.index,
			cbi_batt_info.batt_params.fuel_gauge.manuf_name);
		CPRINTS("output filename:%s", filename);
		util_print_battery_params(&cbi_batt_info);
		fout = fopen(filename, "wb");

		t = (uint8_t *)&cbi_batt_info;
		fwrite(t, sizeof(uint8_t), size, fout);
		fclose(fout);
	}
	return 0;
}
BUILD_ASSERT(CBI_BATTERY_INFO_APPLIED_VERSION <= CBI_BATTERY_INFO_VERSION);
