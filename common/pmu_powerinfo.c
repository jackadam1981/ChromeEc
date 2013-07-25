/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPSChrome powerinfo commands.
 */

#include "console.h"
#include "extpower.h"
#include "host_command.h"
#include "pmu_tpschrome.h"
#include "smart_battery.h"
#include "util.h"

/* FIXME: move all the constants to pmu_tpschrome, make
 * dcdc3, fet output name configurable.
 */
static const struct {
	const char *name;
	int voltage_mv;
	int current_range_ma;
} pmu_fet[] = {
	{"backlight" , 114000, 1100},
	{"video_mid3",   5000,  220},
	{"wwan"      ,   3300, 3300},
	{"sdcard"    ,   3300, 1100},
	{"camera"    ,   3300, 1100},
	{"lcd"       ,   3300, 1100},
	{"video_mid5",   5000, 1100},
}, pmu_dcdc[] = {
	{"p5000", 5050, 5000},
	{"p3300", 3333, 5000},
	{"p1350", 1350, 5000},
};

static const int pmu_voltage_range_mv = 17000;
static const int pmu_ac_sense_range_mv = 33;
static const int pmu_bat_sense_range_mv = 40;
static const int pmu_adc_resolution = 1024;

static inline int calc_voltage(int adc_value, int range_mv)
{
	return adc_value * range_mv / pmu_adc_resolution;
}

static inline int calc_current(int adc_value, int range_ma)
{
	return adc_value * range_ma / pmu_adc_resolution;
}

static inline int calc_current_sr(int adc_value, int sense_resistor_mohm,
				  int range_mv)
{
	return adc_value * range_mv * 1000 / sense_resistor_mohm /
		pmu_adc_resolution;
}

static int command_powerinfo(int argc, char **argv)
{
	int voltage, current;
	int index;

	/* DC to DC converter */
	for (index = 0; index < 3; index++) {
		current = calc_current(
			pmu_adc_read(ADC_IDCDC1 + index, ADC_FLAG_KEEP_ON),
			pmu_dcdc[index].current_range_ma);
		ccprintf("  DCDC%d   : %6d mV, %6d mA (%6d mW) %s\n",
			 index + 1, pmu_dcdc[index].voltage_mv, current,
			 pmu_dcdc[index].name);
	}

	/* FET */
	for (index = 0; index < 7; index++) {
		current = calc_current(
			pmu_adc_read(ADC_IFET1 + index, ADC_FLAG_KEEP_ON),
			pmu_fet[index].current_range_ma);
		ccprintf("  FET%d    : %6d mV, %6d mA (%6d mW) %s\n",
			 index + 1, pmu_fet[index].voltage_mv, current,
			 pmu_dcdc[index].name);
	}

	/* AC input */
	voltage = calc_voltage(
		pmu_adc_read(ADC_VAC, ADC_FLAG_KEEP_ON),
		pmu_voltage_range_mv);
	current = calc_current_sr(
		pmu_adc_read(ADC_IAC, ADC_FLAG_KEEP_ON),
		PMU_SENSE_RAC, pmu_ac_sense_range_mv);
	ccprintf("  AC      : %6d mV, %6d mA (%6d mW)\n", voltage, current,
		 voltage * current / 1000);

	/* Battery charging */
	voltage = calc_voltage(
		pmu_adc_read(ADC_VBAT, ADC_FLAG_KEEP_ON),
		pmu_voltage_range_mv);
	current = calc_current_sr(
		pmu_adc_read(ADC_IBAT, 0),
		PMU_SENSE_RBAT, pmu_bat_sense_range_mv);
	ccprintf("  Charging: %6d mV, %6d mA (%6d mW)\n", voltage, current,
		 voltage * current / 1000);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(powerinfo, command_powerinfo,
		NULL,
		"Show PMU power info",
		NULL);

static int power_command_info(struct host_cmd_handler_args *args)
{
	int bat_charging_current;
	struct ec_response_power_info *r = args->response;

	if (extpower_is_present()) {
		/* Power source = AC */
		r->voltage_ac = calc_voltage(
			pmu_adc_read(ADC_VAC, ADC_FLAG_KEEP_ON),
			pmu_voltage_range_mv);
		r->voltage_system = r->voltage_ac;
		r->current_system = calc_current_sr(
			pmu_adc_read(ADC_IAC, ADC_FLAG_KEEP_ON),
			PMU_SENSE_RAC, pmu_ac_sense_range_mv);
	} else {
		/* Power source == battery */
		r->voltage_ac = 0;
		r->voltage_system = calc_voltage(
			pmu_adc_read(ADC_VBAT, ADC_FLAG_KEEP_ON),
			pmu_voltage_range_mv);
		/* PMU reads charging current. When battery is discharging,
		 * ADC returns 0. Use battery gas guage output instead.
		 */
		battery_current(&bat_charging_current);
		r->current_system = -bat_charging_current;
	}

	/* Ignore USB powerinfo fields. */
	r->usb_dev_type = 0;
	r->usb_current_limit = 0;

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_POWER_INFO, power_command_info, EC_VER_MASK(0));
