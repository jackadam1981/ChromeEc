/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "adc.h"
#include "battery.h"
#include "console.h"
#include "charger.h"
#include "charge_manager.h"
#include "charge_state_v2.h"
#include "apdo.h"
#include "math_util.h"
#include "task.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"
#include "util.h"
#include "usb_pe_sm.h"


#define K_MORE_PWR 96
#define K_LESS_PWR 93
#define PD_REQUEST_STABLE_TIME (10 * SECOND)
#define PD_NEXT_CHECK_TIME (1 * SECOND)

BUILD_ASSERT(K_MORE_PWR > K_LESS_PWR && 100 >= K_MORE_PWR && 100 >= K_LESS_PWR);

/* power-in reference data. */
static timestamp_t timeout[CONFIG_USB_PD_PORT_MAX_COUNT];
static bool is_enabled = true;
static int debug_level;
static int k_less_pwr = K_LESS_PWR;
static int k_more_pwr = K_MORE_PWR;
static int adaptive_mv[CONFIG_USB_PD_PORT_MAX_COUNT];

#define CPRINTF(format, args...) cprintf(CC_USBPD, "APDO " format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, "APDO " format, ##args)

bool apdo_is_enabled(void)
{
	return is_enabled;
}

void apdo_enable(bool en)
{
	is_enabled = en;
}

static bool is_near_limit(int val, int limit)
{
	return val >= (limit * k_more_pwr / 100);
}


/* return true if mv1 is more efficient than mv2 */
static bool is_more_efficient(int mv1, int mv2, int base_mv)
{
	return ABS(mv1 - base_mv) < ABS(mv2 - base_mv);
}

/*
 * Get the most efficient PDO for the battery of the charging port
 *
 * | W\Batt | 1S(3.7V) | 2S(7.4V) | 3S(11.1V) | 4S(14.8V) |
 * --------------------------------------------------------
 * | 0-15W  | 5V       | 9V       | 12V       | 15V       |
 * | 15-27W | 9V       | 9V       | 12V       | 15V       |
 * | 27-36W | 12V      | 12V      | 12V       | 15V       |
 * | 36-45W | 15V      | 15V      | 15V       | 15V       |
 * | 45-60W | 20V      | 20V      | 20V       | 20V       |
 *
 * @return 0 if error occurs, else battery efficient voltage in mV
 */
static int apdo_get_efficient_voltage(void)
{
	int port = charge_manager_get_active_charge_port();
	uint8_t cnt = pd_get_src_cap_cnt(port);
	const uint32_t *src_caps = pd_get_src_caps(port);
	int eff_mv = 0;
	int batt_mv;

	if (battery_design_voltage(&batt_mv))
		return 0;

	for (int i = 0; i < cnt; ++i) {
		int ma, mv, unused;

		pd_extract_pdo_power(src_caps[i], &ma, &mv, &unused);

		/*
		 * If the eff_mv is not picked, or we have more efficient
		 * voltage (less voltage diff)
		 */
		if (eff_mv == 0 || is_more_efficient(mv, eff_mv, batt_mv))
			eff_mv = mv;
	}

	return eff_mv;
}

int apdo_get_desired_input_power(int port, int *vbus, int *input_current)
{
	int active_port;
	enum ec_error_list rv;

	active_port = charge_manager_get_active_charge_port();

	if (active_port == CHARGE_PORT_NONE || port != active_port)
		return 0;

	/* Currently only support solo charger. */
	rv = charger_get_input_current(CHARGER_SOLO, input_current);
	if (rv)
		return 0;

	*vbus = charge_manager_get_vbus_voltage(port);

	if (!*vbus)
		return 0;

	return (*vbus) * (*input_current) / 1000;
}

void apdo_reset_stable(int port)
{
	timeout[port].val = get_time().val + PD_REQUEST_STABLE_TIME;
}

void apdo_init(int port)
{
	adaptive_mv[port] = 0;
	timeout[port].val = 0;
}

int apdo_get_adaptive_voltage(int port)
{
	return adaptive_mv[port];
}

bool apdo_has_new_power_request(int port)
{
	const uint32_t * const src_caps = pd_get_src_caps(port);
	int input_pwr;
	int vbus, input_curr;
	int last_pwr;
	int eff_mv;
	int max_mv = pd_get_max_voltage();
	bool near_pwr_limit = false;
	int last_ma, last_mv;
	int candidate_mv = INT32_MAX;
	int candidate_pwr = 0;
	int ret;
	uint64_t next_wakeup_time;

	if (!apdo_is_enabled())
		return false;

	if (get_time().val < timeout[port].val)
		return false;

	pe_get_last_request(port, &last_ma, &last_mv);

	if (!last_mv)
		return false;


	if (battery_design_voltage(&eff_mv))
		return false;

	last_pwr = last_mv * last_ma / 1000;
	input_pwr = apdo_get_desired_input_power(port, &vbus, &input_curr);

	if (!input_pwr)
		return false;

	near_pwr_limit = is_near_limit(input_pwr, last_pwr) |
			 is_near_limit(input_curr, last_ma);

	if (debug_level)
		CPRINTS("C%d limit=%d last (%dmW %dmV) input (%dmW %dmV %dmA)",
			port, near_pwr_limit, last_pwr, last_mv, input_pwr,
			vbus, input_curr);

	/*
	 * input power might be insufficient, force it to negotiate a more
	 * powerful PDO.
	 */
	if (near_pwr_limit)
		input_pwr = last_pwr + 1;

	for (int i = 0; i < pd_get_src_cap_cnt(port); ++i) {
		int ma, mv, unused;
		int pwr;
		bool efficient;

		pd_extract_pdo_power(src_caps[i], &ma, &mv, &unused);

		if (mv > max_mv)
			continue;

		pwr = ma * mv / 1000;
		efficient = is_more_efficient(mv, candidate_mv, eff_mv);

		if (near_pwr_limit) {
			/* the insufficient case.*/
			if (input_pwr > candidate_pwr &&
			    (pwr > candidate_pwr ||
			     (pwr == candidate_pwr && efficient))) {
				candidate_pwr = pwr;
				candidate_mv = mv;
			} else if (input_pwr <= pwr && efficient) {
				candidate_pwr = pwr;
				candidate_mv = mv;
			}
		} else {
			int adjust_pwr = pwr * k_less_pwr / 100;
			int adjust_cnd_pwr = candidate_pwr * k_less_pwr / 100;

			if (!candidate_pwr) {
				candidate_pwr = pwr;
				candidate_mv = mv;

				/*
				 * if the candidate is insufficient, and we get
				 * one provides more.
				 */
			} else if ((adjust_cnd_pwr < input_pwr &&
				    candidate_pwr < pwr) ||
				   /*
				    * if the candidate is sufficient, and we
				    * pick a more efficient one.
				    */
				   (adjust_cnd_pwr >= input_pwr &&
				    adjust_pwr >= input_pwr && efficient)) {
				candidate_pwr = pwr;
				candidate_mv = mv;
			}
		}
	}

	adaptive_mv[port] = candidate_mv;
	if (!candidate_mv)
		CPRINTS("ERR:CNDMV");
	ret = (candidate_mv != last_mv);
	next_wakeup_time = get_time().val +
			   (ret ? PD_REQUEST_STABLE_TIME : PD_NEXT_CHECK_TIME);

	if (timeout[port].val < next_wakeup_time)
		timeout[port].val = next_wakeup_time;

	if (ret)
		CPRINTS("C%d Req%c", port, candidate_mv > last_mv ? '+' : '-');

	return ret;
}

static int command_apdo(int argc, char **argv)
{
	int port = charge_manager_get_active_charge_port();
	int input_pwr, vbus, input_curr;
	int holder;

	input_pwr = apdo_get_desired_input_power(port, &vbus, &input_curr);
	if (argc == 1) {
		uint32_t last_ma, last_mv;
		int batt_mv;

		if (!is_enabled) {
			ccprintf("APDO Disabled\n");
			return EC_SUCCESS;
		}

		if (port == CHARGE_PORT_NONE) {
			ccprintf("No charger attached\n");
			return EC_SUCCESS;
		}

		battery_design_voltage(&batt_mv);
		pe_get_last_request(port, &last_ma, &last_mv);
		ccprintf("C%d APDO Enabled\n"
			 "Requested: %dmV/%dmA\n"
			 "Measured:  %dmV/%dmA/%dmW\n"
			 "Efficient: %dmV\n"
			 "Batt:      %dmv\n"
			 "K_more:    %d\n"
			 "K_less:    %d\n",
			 port, last_mv, last_ma,
			 vbus, input_curr, input_pwr,
			 apdo_get_efficient_voltage(),
			 batt_mv,
			 k_more_pwr, k_less_pwr);
		return EC_SUCCESS;
	}

	if (argc < 2)
		return EC_ERROR_PARAM2;

	holder = atoi(argv[1]);

	if (!strcasecmp(argv[1], "debug")) {
		debug_level = atoi(argv[2]);
	} else if (!strcasecmp(argv[1], "setkmore")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		k_more_pwr = holder;
	} else if (!strcasecmp(argv[1], "setkless")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		k_less_pwr = holder;
	} else if (!strcasecmp(argv[1], "enable")) {
		apdo_enable(true);
	} else if (!strcasecmp(argv[1], "disable")) {
		apdo_enable(false);
	} else {
		return EC_ERROR_PARAM2;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(apdo, command_apdo,
			"debug|enable|disable|setkmore <int>|setkless <int>",
			"Print/set apdo state.");
