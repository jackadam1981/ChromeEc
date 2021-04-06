/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB common module.
 */
#include "test_util.h"
#include "usb_common.h"

int test_pd_get_cc_state(void)
{
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_3_0, TYPEC_CC_VOLT_RP_3_0),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_3_0, TYPEC_CC_VOLT_RP_1_5),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_3_0, TYPEC_CC_VOLT_RP_DEF),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_1_5, TYPEC_CC_VOLT_RP_3_0),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_1_5, TYPEC_CC_VOLT_RP_1_5),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_1_5, TYPEC_CC_VOLT_RP_DEF),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_DEF, TYPEC_CC_VOLT_RP_3_0),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_DEF, TYPEC_CC_VOLT_RP_1_5),
		PD_CC_DFP_DEBUG_ACC, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_DEF, TYPEC_CC_VOLT_RP_DEF),
		PD_CC_DFP_DEBUG_ACC, "%d");

	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_3_0, TYPEC_CC_VOLT_OPEN),
		PD_CC_DFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_1_5, TYPEC_CC_VOLT_OPEN),
		PD_CC_DFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RP_DEF, TYPEC_CC_VOLT_OPEN),
		PD_CC_DFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_RP_3_0),
		PD_CC_DFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_RP_1_5),
		PD_CC_DFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_RP_DEF),
		PD_CC_DFP_ATTACHED, "%d");

	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RD, TYPEC_CC_VOLT_RD),
		PD_CC_UFP_DEBUG_ACC, "%d");

	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RD, TYPEC_CC_VOLT_RA),
		PD_CC_UFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RD, TYPEC_CC_VOLT_OPEN),
		PD_CC_UFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RA, TYPEC_CC_VOLT_RD),
		PD_CC_UFP_ATTACHED, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_RD),
		PD_CC_UFP_ATTACHED, "%d");

	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RA, TYPEC_CC_VOLT_RA),
		PD_CC_UFP_AUDIO_ACC, "%d");

	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_OPEN),
		PD_CC_NONE, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_OPEN, TYPEC_CC_VOLT_RA),
		PD_CC_NONE, "%d");
	TEST_EQ(pd_get_cc_state(TYPEC_CC_VOLT_RA, TYPEC_CC_VOLT_OPEN),
		PD_CC_NONE, "%d");

	return EC_SUCCESS;
}

int test_pd_extract_pdo_power(void)
{
	uint32_t ma;
	uint32_t mv;

	pd_extract_pdo_power(/* fixed */ 0 << 30 |
				     /* Voltage */ (5000 / 50) << 10 |
				     /* Current */ (3000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 5000, "%d");
	TEST_EQ(ma, 3000, "%d");
	pd_extract_pdo_power(/* fixed */ 0 << 30 |
				     /* Voltage */ (20000 / 50) << 10 |
				     /* Current */ (2600 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 20000, "%d");
	TEST_EQ(ma, 2600, "%d");
	pd_extract_pdo_power(/* fixed */ 0 << 30 |
				     /* Voltage */ (20000 / 50) << 10 |
				     /* Current */ (4000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 20000, "%d");
	TEST_EQ(ma, 3000, "%d"); /* Capped at PD_MAX_CURRENT_MA */
	pd_extract_pdo_power(/* fixed */ 0 << 30 |
				     /* Voltage */ (10000 / 50) << 10 |
				     /* Current */ (4000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 10000, "%d");
	TEST_EQ(ma, 3000, "%d"); /* Capped at PD_MAX_CURRENT_MA */
	pd_extract_pdo_power(/* fixed */ 0 << 30 |
				     /* Voltage */ (21000 / 50) << 10 |
				     /* Current */ (4000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 21000, "%d");
	TEST_EQ(ma, 2857, "%d"); /* Capped at PD_MAX_POWER_MW */

	pd_extract_pdo_power(/* battery */ 1 << 30 |
				     /* Max Voltage */ (5700 / 50) << 20 |
				     /* Min Voltage */ (3300 / 50) << 10 |
				     /* Power */ (12000 / 250),
			     &ma, &mv);
	TEST_EQ(mv, 5700, "%d");
	TEST_EQ(ma, 2105, "%d"); /* 5700 * 2105 ~= 12000 */
	pd_extract_pdo_power(/* battery */ 1 << 30 |
				     /* Max Voltage */ (3300 / 50) << 20 |
				     /* Min Voltage */ (2700 / 50) << 10 |
				     /* Power */ (12000 / 250),
			     &ma, &mv);
	TEST_EQ(mv, 3300, "%d");
	TEST_EQ(ma, 3000, "%d"); /* Capped at PD_MAX_CURRENT_MA */

	pd_extract_pdo_power(/* battery */ 1 << 30 |
				     /* Max Voltage */ (25000 / 50) << 20 |
				     /* Min Voltage */ (5000 / 50) << 10 |
				     /* Power */ (61000 / 250),
			     &ma, &mv);
	TEST_EQ(mv, 25000, "%d");
	TEST_EQ(ma, 2400, "%d"); /* Capped at PD_MAX_POWER_MW */

	pd_extract_pdo_power(/* variable */ 2 << 30 |
				     /* Max Voltage */ (5000 / 50) << 20 |
				     /* Min Voltage */ (3300 / 50) << 10 |
				     /* Current */ (3000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 5000, "%d");
	TEST_EQ(ma, 3000, "%d");
	pd_extract_pdo_power(/* variable */ 2 << 30 |
				     /* Max Voltage */ (20000 / 50) << 20 |
				     /* Min Voltage */ (5000 / 50) << 10 |
				     /* Current */ (2600 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 20000, "%d");
	TEST_EQ(ma, 2600, "%d");
	pd_extract_pdo_power(/* variable */ 2 << 30 |
				     /* Max Voltage */ (20000 / 50) << 20 |
				     /* Min Voltage */ (5000 / 50) << 10 |
				     /* Current */ (4000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 20000, "%d");
	TEST_EQ(ma, 3000, "%d"); /* Capped at PD_MAX_CURRENT_MA */
	pd_extract_pdo_power(/* variable */ 2 << 30 |
				     /* Max Voltage */ (10000 / 50) << 20 |
				     /* Min Voltage */ (3300 / 50) << 10 |
				     /* Current */ (4000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 10000, "%d");
	TEST_EQ(ma, 3000, "%d"); /* Capped at PD_MAX_CURRENT_MA */
	pd_extract_pdo_power(/* variable */ 2 << 30 |
				     /* Max Voltage */ (21000 / 50) << 20 |
				     /* Min Voltage */ (5000 / 50) << 10 |
				     /* Current */ (4000 / 10),
			     &ma, &mv);
	TEST_EQ(mv, 21000, "%d");
	TEST_EQ(ma, 2857, "%d"); /* Capped at PD_MAX_POWER_MW */

	pd_extract_pdo_power(/* augmented */ 3 << 30 |
				     /* Max Voltage */ (5000 / 100) << 17 |
				     /* Min Voltage */ (3300 / 100) << 8 |
				     /* Current */ (3000 / 50),
			     &ma, &mv);
	TEST_EQ(mv, 5000, "%d");
	TEST_EQ(ma, 3000, "%d");
	pd_extract_pdo_power(/* augmented */ 3 << 30 |
				     /* Max Voltage */ (20000 / 100) << 17 |
				     /* Min Voltage */ (3300 / 100) << 8 |
				     /* Current */ (2600 / 50),
			     &ma, &mv);
	TEST_EQ(mv, 20000, "%d");
	TEST_EQ(ma, 2600, "%d");
	pd_extract_pdo_power(/* augmented */ 3 << 30 |
				     /* Max Voltage */ (10000 / 100) << 17 |
				     /* Min Voltage */ (3300 / 100) << 8 |
				     /* Current */ (4000 / 50),
			     &ma, &mv);
	TEST_EQ(mv, 10000, "%d");
	TEST_EQ(ma, 3000, "%d"); /* Capped at PD_MAX_CURRENT_MA */
	pd_extract_pdo_power(/* augmented */ 3 << 30 |
				     /* Max Voltage */ (21000 / 100) << 17 |
				     /* Min Voltage */ (3300 / 100) << 8 |
				     /* Current */ (4000 / 50),
			     &ma, &mv);
	TEST_EQ(mv, 21000, "%d");
	TEST_EQ(ma, 2857, "%d"); /* Capped at PD_MAX_POWER_MW */

	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_pd_get_cc_state);
	RUN_TEST(test_pd_extract_pdo_power);

	test_print_result();
}
