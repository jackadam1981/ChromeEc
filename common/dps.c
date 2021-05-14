/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Dynamic PDO Selection.
 */

#include <stdint.h>

#include "adc.h"
#include "dps.h"
#include "atomic.h"
#include "battery.h"
#include "console.h"
#include "charger.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "charge_state_v2.h"
#include "math_util.h"
#include "task.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"
#include "util.h"
#include "usb_pe_sm.h"


#define K_MORE_PWR 96
#define K_LESS_PWR 93
#define K_SAMPLE 2
#define T_REQUEST_STABLE_TIME (10 * SECOND)
#define T_NEXT_CHECK_TIME (5 * SECOND)

BUILD_ASSERT(K_MORE_PWR > K_LESS_PWR && 100 >= K_MORE_PWR && 100 >= K_LESS_PWR);

/* power-in reference data. */
static timestamp_t timeout;
static bool is_enabled = true;
static int debug_level;
static int adaptive_mv;
static uint32_t dps_flags[CONFIG_USB_PD_PORT_MAX_COUNT];

#define CPRINTF(format, args...) cprintf(CC_USBPD, "DPS " format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, "DPS " format, ##args)

__overridable struct dps_config_t dps_config = {
	.k_less_pwr = K_LESS_PWR,
	.k_more_pwr = K_MORE_PWR,
	.k_sample = K_SAMPLE,
	.t_stable = T_REQUEST_STABLE_TIME,
	.t_check = T_NEXT_CHECK_TIME,
	.is_more_efficient = NULL,
};

int dps_get_adaptive_voltage(int port)
{
	return adaptive_mv;
}

void dps_set_flags(int port, uint32_t flags)
{
	atomic_or(&dps_flags[port], flags);
}

void dps_clr_flags(int port, uint32_t flags)
{
	atomic_clear_bits(&dps_flags[port], flags);
}

bool dps_is_enabled(void)
{
	return is_enabled;
}

void dps_enable(bool en)
{
	bool prev_en = is_enabled;

	is_enabled = en;

	if (is_enabled && !prev_en)
		task_wake(TASK_ID_DPS);
}

/*
 * DPS initialization.
 */
static void dps_init(void)
{
	adaptive_mv = PD_MAX_VOLTAGE_MV;
	timeout.val = get_time().val + dps_config.t_check;
	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; ++i)
		dps_clr_flags(i, DPS_FLAG_ALL);
}

/*
 * DPS reset.
 */
static void dps_reset(void)
{
	dps_init();
}

static bool is_near_limit(int val, int limit)
{
	return val >= (limit * dps_config.k_more_pwr / 100);
}

bool is_more_efficient(int curr_mv, int prev_mv, int batt_mv, int batt_mw,
		       int input_mw)
{
	if (dps_config.is_more_efficient)
		return dps_config.is_more_efficient(curr_mv, prev_mv, batt_mv,
						    batt_mw, input_mw);

	return ABS(curr_mv - batt_mv) < ABS(prev_mv - batt_mv);
}

/*
 * Get the input power of the given port.
 *
 * input_power = battery_desired_power + system_desired_power
 *
 * @param port: The given port
 * @param vbus: VBUS in mV
 * @param input_curr: input current in mA
 *
 * @return input_power of the result of vbus * input_curr in mW
 */
int get_desired_input_power(int *vbus, int *input_current)
{
	int active_port;
	int charger_id;
	enum ec_error_list rv;

	active_port = charge_manager_get_active_charge_port();

	if (active_port == CHARGE_PORT_NONE)
		return 0;

#ifdef CONFIG_CHARGER_SINGLE_CHIP
	charger_id = CHARGER_SOLO;
#else
	charger_id = active_port;
#endif

	/* Currently only support solo charger. */
	rv = charger_get_input_current(charger_id, input_current);
	if (rv)
		return 0;

	*vbus = charge_manager_get_vbus_voltage(active_port);

	if (!*vbus)
		return 0;

	return (*vbus) * (*input_current) / 1000;
}

/*
 * Get the most efficient PDO voltage for the battery of the charging port
 *
 * | W\Batt | 1S(3.7V) | 2S(7.4V) | 3S(11.1V) | 4S(14.8V) |
 * --------------------------------------------------------
 * | 0-15W  | 5V       | 9V       | 12V       | 15V       |
 * | 15-27W | 9V       | 9V       | 12V       | 15V       |
 * | 27-36W | 12V      | 12V      | 12V       | 15V       |
 * | 36-45W | 15V      | 15V      | 15V       | 15V       |
 * | 45-60W | 20V      | 20V      | 20V       | 20V       |
 *
 *
 * @return 0 if error occurs, else battery efficient voltage in mV
 */
int get_efficient_voltage(void)
{
	int eff_mv = 0;
	int batt_mv;
	int batt_pwr;
	int input_pwr, vbus, input_curr;
	const struct batt_params *batt = charger_current_battery_params();

	input_pwr = get_desired_input_power(&vbus, &input_curr);

	if (!input_pwr)
		return 0;

	if (battery_design_voltage(&batt_mv))
		return 0;

	batt_pwr = batt->current * batt->voltage / 1000;

	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; ++i) {
		const int cnt = pd_get_src_cap_cnt(i);
		const uint32_t *src_cdps = pd_get_src_caps(i);

		for (int j = 0; j < cnt; ++j) {
			int ma, mv, unused;

			pd_extract_pdo_power(src_cdps[j], &ma, &mv, &unused);
			/*
			 * If the eff_mv is not picked, or we have more
			 * efficient voltage (less voltage diff)
			 */
			if (eff_mv == 0 ||
			    is_more_efficient(mv, eff_mv, batt_mv, batt_pwr,
					      input_pwr))
				eff_mv = mv;
		}
	}

	return eff_mv;
}

struct pdo_candidate {
	int port;
	int mv;
	int mw;
};

#define UPDATE_CANDIDATE(new_port, new_mv, new_mw) \
	do { \
		cand->port = new_port; \
		cand->mv = new_mv; \
		cand->mw = new_mw; \
	} while (0)

/*
 * Evaluate the system power if a new PD power request is needed.
 *
 * @param struct pdo_candidate: The candidate PDO. (Return value)
 * @return true if a new power request, or false otherwise.
 */
static bool has_new_power_request(struct pdo_candidate *cand)
{
	int vbus, input_curr, input_pwr;
	int last_pwr;
	int batt_pwr, batt_mv;
	int max_mv = pd_get_max_voltage();
	bool near_pwr_limit = false;
	int last_ma, last_mv;
	int input_curr_limit;
	int active_port = charge_manager_get_active_charge_port();
	const struct batt_params *batt = charger_current_battery_params();

	/* set a default value in case it early returns. */
	cand->port = CHARGE_PORT_NONE;
	cand->mv = INT32_MAX;
	cand->mw = 0;

	if (!dps_is_enabled())
		return false;

	if (active_port == CHARGE_PORT_NONE)
		return false;

	last_mv = pd_get_requested_voltage(active_port);
	last_ma = pd_get_requested_current(active_port);

	if (!last_mv)
		return false;

	if (battery_design_voltage(&batt_mv))
		return false;

	last_pwr = last_mv * last_ma / 1000;
	batt_pwr = batt->current * batt->voltage / 1000;
	input_pwr = get_desired_input_power(&vbus, &input_curr);

	if (!input_pwr)
		return false;

	if (!charger_get_input_current_limit(active_port, &input_curr_limit))
		/* set as last requested mA if we're unable to get the limit. */
		input_curr_limit = last_ma;

	/*
	 * If the current(power) is either close to the PDO current(power) or
	 * the charger input current limit, mark it as close limit.
	 */
	near_pwr_limit =
		is_near_limit(input_pwr, last_pwr) ||
		is_near_limit(input_curr, MIN(last_ma, input_curr_limit));

	if (debug_level)
		CPRINTS("C%d limit=%d last (%dmW %dmV) input (%dmW %dmV %dmA)",
			active_port, near_pwr_limit, last_pwr, last_mv,
			input_pwr, vbus, input_curr);

	/*
	 * input power might be insufficient, force it to negotiate a more
	 * powerful PDO.
	 */
	if (near_pwr_limit)
		input_pwr = last_pwr + 1;

	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; ++i) {
		const uint32_t * const src_cdps = pd_get_src_caps(i);

		for (int j = 0; j < pd_get_src_cap_cnt(i); ++j) {
			int ma, mv, unused;
			int mw;
			bool efficient;

			pd_extract_pdo_power(src_cdps[j], &ma, &mv, &unused);

			if (mv > max_mv)
				continue;

			mw = ma * mv / 1000;
			efficient = is_more_efficient(mv, cand->mv, batt_mv,
						      batt_pwr, input_pwr);

			if (near_pwr_limit) {
				/* the insufficient case.*/
				if (input_pwr > cand->mw &&
				    (mw > cand->mw ||
				     (mw == cand->mw && efficient))) {
					UPDATE_CANDIDATE(i, mv, mw);
				} else if (input_pwr <= mw && efficient) {
					UPDATE_CANDIDATE(i, mv, mw);
				}
			} else {
				int adjust_pwr =
					mw * dps_config.k_less_pwr / 100;
				int adjust_cand_mw =
					cand->mw * dps_config.k_less_pwr / 100;

				/* Pick if we don't have a candidate yet. */
				if (!cand->mw) {
					UPDATE_CANDIDATE(i, mv, mw);
					/*
					 * if the candidate is insufficient, and
					 * we get one provides more.
					 */
				} else if ((adjust_cand_mw < input_pwr &&
					    cand->mw < mw) ||
					   /*
					    * if the candidate is sufficient,
					    * and we pick a more efficient one.
					    */
					   (adjust_cand_mw >= input_pwr &&
					    adjust_pwr >= input_pwr &&
					    efficient)) {
					UPDATE_CANDIDATE(i, mv, mw);
				}
			}
		}
	}

	if (!cand->mv)
		CPRINTS("ERR:CNDMV");

	return (cand->mv != last_mv);
}

void dps_update_stabilized_time(int port)
{
	if (dps_flags[port] & DPS_FLAG_NEW_CONTRACT)
		timeout.val = get_time().val + dps_config.t_stable;

	dps_clr_flags(port, DPS_FLAG_NEW_CONTRACT);
}

void dps_task(void *u)
{
	struct pdo_candidate last_cand = {CHARGE_PORT_NONE, 0, 0};
	int sample_count = 0;

	dps_init();

	while (1) {
		struct pdo_candidate curr_cand = {CHARGE_PORT_NONE, 0, 0};
		bool has_srccap = false;
		timestamp_t now;

		if (!is_enabled) {
			dps_reset();
			task_wait_event(-1);
			continue;
		}

		for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; ++i) {
			if (pd_is_connected(i) &&
			    pd_get_power_role(i) == PD_ROLE_SINK &&
			    pd_get_src_cap_cnt(i) > 0) {
				has_srccap = true;
				break;
			}
		}

		if (!has_srccap) {
			dps_reset();
			task_wait_event(-1);
			continue;
		}

		now = get_time();
		if (now.val < timeout.val) {
			task_wait_event(timeout.val - now.val);
			continue;
		}

		if (!has_new_power_request(&curr_cand)) {
			sample_count = 0;
		} else {
			if (last_cand.port == curr_cand.port &&
			    last_cand.mv == curr_cand.mv &&
			    last_cand.mw == curr_cand.mw)
				sample_count++;
			else
				sample_count = 1;
		}

		if (sample_count == dps_config.k_sample) {
			adaptive_mv = curr_cand.mv;
			pd_dpm_request(curr_cand.port,
				       DPM_REQUEST_NEW_POWER_LEVEL);
			sample_count = 0;
		}

		last_cand.port = curr_cand.port;
		last_cand.mv = curr_cand.mv;
		last_cand.mw = curr_cand.mw;

		timeout.val = now.val + dps_config.t_check;
	}
}

static int command_dps(int argc, char **argv)
{
	int port = charge_manager_get_active_charge_port();
	int input_pwr, vbus, input_curr;
	int holder;

	if (argc == 1) {
		uint32_t last_ma, last_mv;
		int batt_mv;

		ccprintf("k_more=%d k_less=%d k_sample=%d\n",
			 dps_config.k_more_pwr, dps_config.k_less_pwr,
			 dps_config.k_sample);
		ccprintf("t_stable=%d t_check=%d\n",
			 dps_config.t_stable / SECOND,
			 dps_config.t_check / SECOND);
		if (!is_enabled) {
			ccprintf("DPS Disabled\n");
			return EC_SUCCESS;
		}

		if (port == CHARGE_PORT_NONE) {
			ccprintf("No charger attached\n");
			return EC_SUCCESS;
		}

		battery_design_voltage(&batt_mv);
		input_pwr = get_desired_input_power(&vbus, &input_curr);
		last_mv = pd_get_requested_voltage(port);
		last_ma = pd_get_requested_current(port);
		ccprintf("C%d DPS Enabled\n"
			 "Requested: %dmV/%dmA\n"
			 "Measured:  %dmV/%dmA/%dmW\n"
			 "Efficient: %dmV\n"
			 "Batt:      %dmv\n",
			 port, last_mv, last_ma,
			 vbus, input_curr, input_pwr,
			 get_efficient_voltage(),
			 batt_mv);
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "enable")) {
		dps_enable(true);
		return EC_SUCCESS;
	} else if (!strcasecmp(argv[1], "disable")) {
		dps_enable(false);
		return EC_SUCCESS;
	} else if (argc == 2) {
		return EC_ERROR_PARAM2;
	}

	if (argc > 3)
		return EC_ERROR_PARAM4;

	if (!strcasecmp(argv[1], "debug")) {
		debug_level = atoi(argv[2]);
	} else if (!strcasecmp(argv[1], "setkmore")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		if (holder > 100 || holder < 0 ||
		    holder < dps_config.k_less_pwr)
			return EC_ERROR_INVAL;

		dps_config.k_more_pwr = holder;
	} else if (!strcasecmp(argv[1], "setkless")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		if (holder > 100 || holder < 0 ||
		    holder > dps_config.k_more_pwr)
			return EC_ERROR_INVAL;

		dps_config.k_less_pwr = holder;
	} else if (!strcasecmp(argv[1], "setksample")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		if (holder <= 0)
			return EC_ERROR_INVAL;

		dps_config.k_sample = holder;
	} else if (!strcasecmp(argv[1], "settcheck")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		if (holder < 0)
			return EC_ERROR_INVAL;
		dps_config.t_check = holder * SECOND;
	} else if (!strcasecmp(argv[1], "settstable")) {
		holder = atoi(argv[2]);
		if (!holder)
			return EC_ERROR_PARAM2;
		if (holder < 0)
			return EC_ERROR_INVAL;

		dps_config.t_stable = holder * SECOND;
	} else {
		return EC_ERROR_PARAM2;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dps, command_dps,
			"\tenable|disable|debug <int>\n"
			"\t\tset(kmore|kless|ksample|tstable|tcheck) <int>",
			"Print/set Dynamic PDO Selection state.");
