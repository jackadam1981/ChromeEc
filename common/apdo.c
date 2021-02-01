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

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

#define K_MORE_PWR 96
#define K_LESS_PWR 93

BUILD_ASSERT(K_MORE_PWR > K_LESS_PWR && K_MORE_PWR <= 100 && K_LESS_PWR <= 100);

/* power-in reference data. */
static timestamp_t last_req_time;
static int stable_current;
static bool is_enabled = true;
static int input_power_mw;
/* current requested voltage by the system */
static int curr_req_mv[CONFIG_USB_PD_PORT_MAX_COUNT];
static int curr_req_ma[CONFIG_USB_PD_PORT_MAX_COUNT];
static int k_less_pwr = K_LESS_PWR;
static int k_more_pwr = K_MORE_PWR;
static uint8_t flags[CONFIG_USB_PD_PORT_MAX_COUNT];

enum APDO_FLAGS {
	APDO_FLAGS_MORE_PWR = BIT(0),
	APDO_FLAGS_LESS_PWR = BIT(1),
};

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

bool apdo_is_enabled(void)
{
	return is_enabled;
}

void apdo_control_enable(bool en)
{
	is_enabled = en;
}

bool apdo_is_input_current_stable(void)
{
	return stable_current == CHARGE_CURRENT_UNINITIALIZED;
}

/* #define SAMPLE_COUNT (1>>3)
 * static uint32_t input_curr_buf[CONFIG_USB_PD_PORT_MAX_COUNT][SAMPLE_COUNT];
 * static uint8_t input_curr_ptr[CONFIG_USB_PD_PORT_MAX_COUNT];
 * static uint32_t input_curr_moving_sum[CONFIG_USB_PD_PORT_MAX_COUNT];
 *
 * #define NEXT_PTR(x) (((x)+1) & SAMPLE_COUNT) */

void apdo_init(void)
{
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
		int ma, mv;

		pd_extract_pdo_power(src_caps[i], &ma, &mv);

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

	/* This currently only support solo charger. */
	rv = charger_get_input_current(CHARGER_SOLO, input_current);
	if (rv)
		return 0;

	*vbus = charge_manager_get_vbus_voltage(port);

	if (!*vbus)
		return 0;

	return (*vbus) * (*input_current) / 1000;
}

void apdo_reset_timer(void)
{
	last_req_time = get_time();
}

#define PD_REQUEST_STABLE_TIME (10 * SECOND)

void apdo_try_new_power_request(void)
{
	int is_pd_supply = charge_manager_get_supplier() == CHARGE_SUPPLIER_PD;
	int port = charge_manager_get_active_charge_port();
	const uint32_t * const src_caps = pd_get_src_caps(port);
	int input_power_mw;
	int vbus, input_curr;
	int curr_req_pwr;
	int eff_mv;
	int max_mv = pd_get_max_voltage();
	bool near_pwr_limit = false;

	if (!is_pd_supply ||
	    charge_manager_get_active_charge_port() == CHARGE_PORT_NONE)
		return;

	if (get_time().val < last_req_time.val + PD_REQUEST_STABLE_TIME)
		return;

	flags[port] = 0;

	curr_req_pwr = curr_req_mv[port] * curr_req_ma[port] / 1000;
	input_power_mw = apdo_get_desired_input_power(port, &vbus, &input_curr);


	near_pwr_limit = is_near_limit(input_power_mw, curr_req_pwr) |
			is_near_limit(input_curr, curr_req_ma[port]);

	eff_mv = apdo_get_efficient_voltage();

	for (int i = 0; i < pd_get_src_cap_cnt(port); ++i) {
		int ma, mv;
		int pdo_pwr;

		pd_extract_pdo_power(src_caps[i], &ma, &mv);
		pdo_pwr = ma * mv / 1000;

		if (input_power_mw < pdo_pwr * k_less_pwr / 100 &&
		    pdo_pwr <= curr_req_pwr &&
		    is_more_efficient(mv, curr_req_mv[port], eff_mv))
			flags[port] |= APDO_FLAGS_LESS_PWR;

		if (near_pwr_limit && curr_req_pwr <= pdo_pwr && mv <= max_mv &&
		    pdo_pwr <= PD_MAX_POWER_MW &&
		    is_more_efficient(mv, curr_req_mv[port], eff_mv))
			flags[port] |= APDO_FLAGS_MORE_PWR;
	}

	if (flags[port]) {
		CPRINTS("\033[31mC%d: Req %s%s pwr. "
			"Expect(%dmV %dmA %dmW) Was(%dmV %dmA %dmW)\033[0m",
			port, flags[port] & APDO_FLAGS_MORE_PWR ? "more" : "",
			flags[port] & APDO_FLAGS_LESS_PWR ? "less" : "", vbus,
			input_curr, input_power_mw, curr_req_mv[port],
			curr_req_ma[port], curr_req_pwr);
		apdo_reset_timer();
		pd_set_new_power_request(port);
	}
}


void apdo_set_request_power(int port, int mv, int ma)
{
	curr_req_mv[port] = mv;
	curr_req_ma[port] = ma;
}

int apdo_find_pdo_index(int port, uint32_t src_cap_cnt,
			const uint32_t *const src_caps, int max_mv,
			uint32_t *selected_pdo)
{
	int active_port;
	int curr_req_pwr;
	/* The most efficient voltage the current SrcCap provides. */
	/* int32_t pwr_upper = INT32_MAX, pwr_lower = 0; */
	/* int8_t pdo_idx_upper = -1, pdo_idx_lower = -1; */
	/* int32_t mv_upper = INT32_MAX, mv_lower = 0; */
	int candidate_idx = -1;
	int candidate_pwr = 0;
	int candidate_mv = INT32_MAX;
	int state_of_charge;
	int eff_mv;
	int vbus, input_curr;
	/* int k; */

	if (!is_enabled)
		goto failed;

	active_port = charge_manager_get_active_charge_port();
	/*
	 * There is no charging port, we should use normal evaluation for the
	 * first round, and evaluate the PDO in the next round.
	 */
	if (active_port == CHARGE_PORT_NONE) {
		ccprintf("\033[32mERR:active_port==NONE\033[0m\n");
		goto failed;
	}

	/* The port is not the charging port, re-evaluate this later.  */
	if (port != active_port) {
		ccprintf("\033[32mERR:active_port!=port\033[0m\n");
		goto failed;
	}

	if (charge_manager_get_supplier() != CHARGE_SUPPLIER_PD) {
		ccprintf("\033[32mERR:supplier!=PD\033[0m\n");
		goto failed;
	}

	/* No battery -> no need to APDO. */
	if (battery_is_present() != BP_YES) {
		ccprintf("\033[32mERR:!BP_YES\033[0m\n");
		goto failed;
	}

	if (battery_state_of_charge_abs(&state_of_charge))
		goto failed;

	/* Is the battery is low, do not being adaptive. */
	if (state_of_charge <= BATTERY_LEVEL_LOW) {
		ccprintf("\033[32mERR:BP_LOW\033[0m\n");
		goto failed;
	}

	if (!flags[port]) {
		ccprintf("\033[32mERR:!FLAGS\033[0m\n");
		goto failed;
	}

	eff_mv = apdo_get_efficient_voltage();
	if (!eff_mv) {
		ccprintf("\033[32mERR:!EFFMV\033[0m\n");
		goto failed;
	}

	input_power_mw = apdo_get_desired_input_power(port, &vbus, &input_curr);
	if (!input_power_mw) {
		ccprintf("\033[32mERR:!INPUT_PWR\033[0m\n");
		goto failed;
	}

	curr_req_pwr = curr_req_mv[port] * curr_req_ma[port] / 1000;

	if (flags[port] & APDO_FLAGS_MORE_PWR)
		input_power_mw = curr_req_pwr + 1;

	for (uint8_t i = 0; i < src_cap_cnt; ++i) {
		int32_t ma, mv;
		int32_t pdo_pwr;

		pd_extract_pdo_power(src_caps[i], (uint32_t *)&ma,
				     (uint32_t *)&mv);

		if (mv > max_mv)
			continue;

		/*
		 * If the input current hits the PDO current limit,
		 * this implies the PDO power might be insufficinet, thus we
		 * should transist to a higher voltage. [1]
		 *
		 * If it's a low to high voltage transistion, ensure the
		 * input power is closed to the limit of the PDO
		 * can supply.
		 *
		 * If it's a high to low voltage transistion, ensure the
		 * input power has reached a lower value then we
		 * transist to it in case of frequently voltage
		 * trasnsition in the boarder of the supply power.
		 */

		pdo_pwr = ma * mv / 1000;

		/* if (flags[port] & APDO_FLAGS_MORE_PWR) {
		 *         if (pdo_pwr >= input_power_mw) {
		 *                 if (ABS(mv - eff_mv) >=
		 *                     ABS(candidate_mv - eff_mv)) {
		 *                         candidate_mv = mv;
		 *                         candidate_pwr = pdo_pwr;
		 *                         candidate_idx = i;
		 *                 }
		 *         } else if (input_power_mw >= candidate_pwr &&
		 *                    pdo_pwr >= candidate_pwr) {
		 *                 candidate_mv = mv;
		 *                 candidate_pwr = pdo_pwr;
		 *                 candidate_idx = i;
		 *         }
		 * } else if (flags[port] & APDO_FLAGS_LESS_PWR) {
		 *         if (pdo_pwr >= input_power_mw) {
		 *                 if (ABS(mv - eff_mv) >=
		 *                     ABS(candidate_mv - eff_mv)) {
		 *                         candidate_mv = mv;
		 *                         candidate_pwr = pdo_pwr;
		 *                         candidate_idx = i;
		 *                 }
		 *         }
		 * } */

		if (pdo_pwr >= input_power_mw) {
			if (is_more_efficient(mv, candidate_mv, eff_mv)) {
				candidate_mv = mv;
				candidate_pwr = pdo_pwr;
				candidate_idx = i;
			}

		} else if (flags[port] & APDO_FLAGS_MORE_PWR) {
			if (input_power_mw >= candidate_pwr &&
			    pdo_pwr >= candidate_pwr) {
					candidate_mv = mv;
					candidate_pwr = pdo_pwr;
					candidate_idx = i;
			}
		}

/*                 if (eff_mv == mv && pdo_pwr >= input_power_mw) {
 *                         if (flags[port] & APDO_FLAGS_MORE_PWR) {
 *                                 *selected_pdo = src_caps[i];
 *                                 flags[port] = 0;
 *                                 return i;
 *                         }
 *                         pwr_lower = pdo_pwr;
 *                         pdo_idx_lower = i;
 *                         mv_lower = mv;
 *                 }
 *
 *                 if (pdo_pwr >= input_power_mw && pdo_pwr <= pwr_upper &&
 *                     ABS(mv - eff_mv) < ABS(mv_upper - eff_mv)) {
 *                         pwr_upper = pdo_pwr;
 *                         pdo_idx_upper = i;
 *                         mv_upper = mv;
 *                 }
 *
 *                 if (pdo_pwr <= input_power_mw && pdo_pwr >= pwr_lower &&
 *                     ABS(mv - eff_mv) < ABS(mv_lower - eff_mv)) {
 *                         pwr_lower = pdo_pwr ;
 *                         pdo_idx_lower = i;
 *                         mv_lower = mv;
 *                 } */

		/* CPRINTS("\033[35mAPDO: upper=(mv %d pwr %d) lower=(mv %d pwr %d)\033[0m",
		 *         mv_upper, pwr_upper, mv_lower, pwr_lower); */
	}

	CPRINTS("\033[35mAPDO: Req (%dmV %dmW)\033[0m", candidate_mv, candidate_pwr);

	//flags[port] = 0;
	*selected_pdo = src_caps[candidate_idx];
	return candidate_idx;

	/* if (pdo_idx_upper != -1) {
	 *         *selected_pdo = src_caps[pdo_idx_upper];
	 *         flags[port] = 0;
	 *         return pdo_idx_upper;
	 * } else {
	 *         *selected_pdo = src_caps[pdo_idx_lower];
	 *         flags[port] = 0;
	 *         return pdo_idx_lower;
	 * } */

failed:
	flags[port] = 0;
	return -1;
}

/*
void apdo_task(void *u)
{
	while(1) {
		int vbus, input_current;
		int input_power;

		msleep(500);

		vbus = charge_manager_get_vbus_voltage(0);

		if (charger_get_input_current(0, &input_current)) {
			CPRINTF("ERR:ICURR%d\n", 0);
			continue;
		}

		input_power = vbus * input_current / 1000;
		CPRINTS("Power %5d, VBUS %6d, InputCurrent %6d", input_power,
			vbus, input_current);
	}
}
*/

/*
 * enable disable
 * print
 */
static int command_apdo(int argc, char **argv)
{
	int port = charge_manager_get_active_charge_port();
	int input_power_mw, vbus, input_curr;

	if (port == CHARGE_PORT_NONE) {
		ccprintf("No chger attached\n");
		return EC_SUCCESS;
	}

	input_power_mw = apdo_get_desired_input_power(port, &vbus, &input_curr);
	if (argc == 1) {
		if (!is_enabled) {
			ccprintf("Disabled\n");
			return EC_SUCCESS;
		}

		ccprintf("C%d PDO %ddmV/%dmA "
			 "Input %dmW %dmV/%dmA "
			 "Eff %dmV\n",
			 port, curr_req_mv[port], curr_req_ma[port],
			 input_power_mw, vbus, input_curr,
			 apdo_get_efficient_voltage());
		return EC_SUCCESS;
	}

	if (argc != 2) {
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(apdo, command_apdo, "", "Print/set apdo state.");
