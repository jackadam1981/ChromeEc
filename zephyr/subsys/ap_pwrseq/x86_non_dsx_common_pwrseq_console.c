/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

#include "gpio.h"
#include "gpio/gpio_int.h"
#include <zephyr/drivers/espi.h>
#define espi_dev DEVICE_DT_GET(DT_CHOSEN(cros_ec_espi))

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

/* Console commands */
static int powerinfo_handler(const struct shell *shell, size_t argc,
			     char **argv)
{
	enum power_states_ndsx state = pwr_sm_get_state();

	shell_fprintf(shell, SHELL_INFO, "power state %d = %s, in 0x%04x\n",
		      state, pwr_sm_get_state_name(state), power_get_signals());
	return 0;
}

SHELL_CMD_REGISTER(powerinfo, NULL, NULL, powerinfo_handler);

static int powerindebug_handler(const struct shell *shell, size_t argc,
				char **argv)
{
	int i;
	char *e;
	power_signal_mask_t current;

	/* If one arg, set the mask */
	if (argc == 2) {
		int m = strtol(argv[1], &e, 0);

		if (*e)
			return -EINVAL;

		power_set_debug(m);
	}

	/* Print the mask */
	current = power_get_signals();
	shell_fprintf(shell, SHELL_INFO, "power in:   0x%05x\n", current);
	shell_fprintf(shell, SHELL_INFO, "debug mask: 0x%05x\n",
		      power_get_debug());

	/* Print the decode */
	shell_fprintf(shell, SHELL_INFO, "bit meanings:\n");
	for (i = 0; i < POWER_SIGNAL_COUNT; i++) {
		power_signal_mask_t mask = POWER_SIGNAL_MASK(i);
		bool valid = (power_signal_get(i) >= 0);

		shell_fprintf(shell, SHELL_INFO, "  0x%05x %d%s %s\n", mask,
			      (current & mask) ? 1 : 0, valid ? " " : "!",
			      power_signal_name(i));
	}

	{
	uint8_t vw1, vw2, vw3;
	int s0 = gpio_get_level(GPIO_PCH_SLP_S0_L);
	espi_receive_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SLP_S5, &vw1),
	espi_receive_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SLP_S4, &vw2),
	espi_receive_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SLP_S3, &vw3);
	ccprintf("slp_s5=%d, slp_s4=%d, slp_s3=%d, slp_s0=%d\n", vw1, vw2, vw3, s0);
	}

	return 0;
};

SHELL_CMD_REGISTER(powerindebug, NULL, "[mask] Get/set power input debug mask",
		   powerindebug_handler);

void powerindebug_func(void)
{
	int i;
	power_signal_mask_t current;
	//powerindebug_handler(NULL, 1, NULL);
	//
	current = power_get_signals();
	ccprintf("power in:   0x%05x\n", current);
	ccprintf("debug mask: 0x%05x\n", power_get_debug());
	/* Print the decode */
	ccprintf("bit meanings:\n");
	for (i = 0; i < POWER_SIGNAL_COUNT; i++) {
		power_signal_mask_t mask = POWER_SIGNAL_MASK(i);
		bool valid = (power_signal_get(i) >= 0);

		ccprintf(" 0x%05x %d%s %s\n", mask,
			      (current & mask) ? 1 : 0, valid ? " " : "!",
			      power_signal_name(i));
	}
	{
	uint8_t vw1, vw2, vw3;
	int s0 = gpio_get_level(GPIO_PCH_SLP_S0_L);
	espi_receive_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SLP_S5, &vw1),
	espi_receive_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SLP_S4, &vw2),
	espi_receive_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SLP_S3, &vw3);
	ccprintf("slp_s5=%d, slp_s4=%d, slp_s3=%d, slp_s0=%d\n", vw1, vw2, vw3, s0);
	}
}

static int apshutdown_handler(const struct shell *shell, size_t argc,
			      char **argv)
{
	ap_power_force_shutdown(AP_POWER_SHUTDOWN_CONSOLE_CMD);
	return 0;
}

SHELL_CMD_REGISTER(apshutdown, NULL, NULL, apshutdown_handler);

static int apreset_handler(const struct shell *shell, size_t argc, char **argv)
{
	ap_power_reset(AP_POWER_SHUTDOWN_CONSOLE_CMD);
	return 0;
}

SHELL_CMD_REGISTER(apreset, NULL, NULL, apreset_handler);

/* End of console commands */
