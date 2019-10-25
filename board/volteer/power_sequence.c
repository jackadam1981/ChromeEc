/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Volteer board-specific power sequencing
 * Power sequencing is largely done by the platform automatically.
 * However, if platform power sequencing is buggy or needs tuning,
 * resistors can be stuffed on the board to allow the EC full control over
 * the power sequencing.
 */

#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "system.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

static void board_wakeup(void)
{
	CPRINTS("%s", __func__);
	/*
	 * PP5000_USB_AG - normally enabled automatically by EN_3300_AG which
	 * is connected to the PSL_OUT of the Nuvoton.
	 *
	 * Assert the signal high during wakeup, deassert at hibernate
	 */
	CPRINTS("enable EN_PP5000_USB_AG");
	gpio_set_level(GPIO_EN_PP5000_USB_AG, 1);
}
DECLARE_HOOK(HOOK_INIT, board_wakeup, HOOK_PRIO_DEFAULT);

__override void board_hibernate_late(void)
{
	CPRINTS("%s", __func__);
	/* Disable PP5000_USB_AG on hibernate */
	CPRINTS("enable EN_PP5000_USB_AG");
	gpio_set_level(GPIO_EN_PP5000_USB_AG, 0);
}

/* Called during S5 -> S3 transition */
static void board_chipset_startup(void)
{
	CPRINTS("%s", __func__);

	/*
	 * Power on 1.8V rail,
	 * tPCH06, minimum 200us from P-P3300_DSW stable to before
	 * VCCPRIM_1P8 starting up.
	 *
	 * The transition to S5 and (S3) is gated by SLP_SUS#, which Tiger Lake
	 * internally delays a minimum of 95 ms from DSW_PWROK. So no delay
	 * needed here.
	 */
	CPRINTS("enable EN_PP1800_A");
	gpio_set_level(GPIO_EN_PP1800_A, 1);

	/*
	 * Power on VCCIN Aux - no delay specified, but must follow VCCPRIM_1P8
	 */
	CPRINTS("enable EN_PPVAR_VCCIN_AUX");
	gpio_set_level(GPIO_EN_PPVAR_VCCIN_AUX, 1);

	/*
	 * Power on bypass rails - must be turned on after VCCIN aux is stable
	 */
	msleep(2);
	CPRINTS("enable bypass rails");
	gpio_set_level(GPIO_EN_VNN_BYPASS, 1);
	gpio_set_level(GPIO_EN_P1050_BYPASS, 1);

	/*
	 * Power on VCCST - must be gated by SLP_S3#.  No order with respect to
	 * other power signals specified.
	 */
	CPRINTS("enable EN_PP1050_ST_S0");
	gpio_set_level(GPIO_EN_PP1050_ST_S0, 1);

	/*
	 * Power on DDR railS
	 * No delay needed - SLP_S4# already guaranteed to be de-asserted.
	 */
	CPRINTS("enable DDR rails");
	gpio_set_level(GPIO_EN_DRAM_VDD1, 1);
	gpio_set_level(GPIO_EN_DRAM_VDDQ, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);

/* Called during S3 -> S0 and S0ix -> S0 transition */
static void board_chipset_resume(void)
{
	CPRINTS("%s", __func__);
	/*
	 * Power on VCCSTG rail to Tiger Lake, no PG signal available
	 */
	CPRINTS("enable EN_PP1050_STG");
	gpio_set_level(GPIO_EN_PP1050_STG, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);


/* Called during S0 -> S0ix transition */
static void board_chipset_suspend(void)
{
	CPRINTS("%s", __func__);
	/* Power down VCCSTG rail */
	CPRINTS("disable EN_PP1050_STG");
	gpio_set_level(GPIO_EN_PP1050_STG, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

/* Called during S3 -> S5 transition */
static void board_chipset_shutdown(void)
{
	CPRINTS("%s", __func__);

	/*
	 * S0 to G3 sequence 1 of 2 (shared between Deep Sx and non-Deep Sx)
	 *  TigerLake Rail    Net Name
	 *  VCCSTG            PP1050_STG_S0
	 *  DDR_VDDQ          PP0600_VDDQ
	 *  VCCST             PP1050_ST_S0
	 *  DDR_VPP           PP1800_DRAM
	 */
	gpio_set_level(GPIO_EN_PP1050_STG, 0);
	gpio_set_level(GPIO_EN_DRAM_VDDQ, 0);
	gpio_set_level(GPIO_EN_PP1050_ST_S0, 0);
	gpio_set_level(GPIO_EN_DRAM_VDD1, 0);

	/*
	 * S0 to G3 sequence 2 of 2 (non-Deep Sx)
	 *  TigerLake Name    Net Name
	 *  VCCPRIM_3P3       PP3300_A
	 *  VCCDSW_3P3        VCCDSW_3P3 (PP3300_A)
	 *  V5.0A             PP5000_A
	 *  VCCPRIM_1P8       PP1800_A
	 *  VCCIN_AUX         PPVAR_VCCIN_AUX
	 *  VNN_BYPASS        PPVAR_VNN_BYPASS
	 *  V1.05A_BYPASS     PP1050_A_BYPASS
	 */

	/* Ice Lake shutdown already sequences first 3 rails above. */
	chipset_force_shutdown(CHIPSET_SHUTDOWN_G3);

	gpio_set_level(GPIO_EN_PP1800_A, 0);
	gpio_set_level(GPIO_EN_PPVAR_VCCIN_AUX, 0);
	gpio_set_level(GPIO_EN_VNN_BYPASS, 0);
	gpio_set_level(GPIO_EN_P1050_BYPASS, 0);


}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

/**
 * Handle C10_GATE transitions - see VCCSTG enable logic (figure 232, page 406)
 * in Tiger Lake PDG, revision 1.0.
 *
 * This function can be promoted to common TigerLake power file.
 */
void c10_gate_change(enum gpio_signal signal)
{
	/* Pass through CPU_C10_GATE_L as enable for VCCSTG rail */
	int c10_gate_in = gpio_get_level(signal);
	int vccstg_out = gpio_get_level(GPIO_EN_PP1050_STG);

	if (vccstg_out == c10_gate_in)
		return;

	gpio_set_level(GPIO_EN_PP1050_STG, c10_gate_in);
}





