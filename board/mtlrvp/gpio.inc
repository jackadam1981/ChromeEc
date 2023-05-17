/* -*- mode:c -*-
 *
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel MTL-P-RVP board-specific configuration */

/* Power sequencing interrupts */
GPIO_INT(ALL_SYS_PWRGD, PIN(7, 0), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(RSMRST_PWRGD, PIN(6, 6), GPIO_INT_BOTH | GPIO_SEL_1P8V,
	 power_signal_interrupt)
GPIO_INT(PCH_SLP_S0_N_EC, PIN(A, 1), GPIO_INT_BOTH,
	 power_signal_interrupt) /* 1.8V */

#ifndef CONFIG_HOSTCMD_ESPI_VW_SLP_S3
GPIO_INT(PM_SLP_S3_N_EC, PIN(B, 0), GPIO_INT_BOTH, power_signal_interrupt) /* 1.8V
									    */
#else
GPIO(PM_SLP_S3_N, PIN(B, 0), GPIO_INPUT)
#endif

#ifndef CONFIG_HOSTCMD_ESPI_VW_SLP_S4
GPIO_INT(PM_SLP_S4_N_EC, PIN(A, 5), GPIO_INT_BOTH, power_signal_interrupt) /* 1.8V
									    */
#else
GPIO(PM_SLP_S4_N, PIN(A, 5), GPIO_INPUT)
#endif

GPIO_INT(SMC_ONOFF_N, PIN(D, 2), GPIO_INT_BOTH, power_button_interrupt)
GPIO_INT(SMC_LID, PIN(0, 1), GPIO_INT_BOTH, lid_interrupt)

/* Host communication GPIOs */
GPIO(PCH_WAKE_N, PIN(7, 4), GPIO_ODR_HIGH)
#ifndef CONFIG_HOST_INTERFACE_ESPI
GPIO(ESPI_RST_N, PIN(5, 4), GPIO_INPUT) /* 1.8V */
GPIO(PLT_RST_L, PIN(A, 2), GPIO_INPUT) /* 1.8V, PCH_PLTRST_L */
#endif

/* Power sequencing GPIOs */
GPIO(SYS_RST_ODL_EC, PIN(C, 5), GPIO_ODR_HIGH)
GPIO(PM_RSMRST_R_N, PIN(A, 4), GPIO_OUT_LOW) /* 1.8V */
GPIO(PM_PWRBTN_N_EC, PIN(D, 4), GPIO_ODR_HIGH)
GPIO(EC_DS3_R, PIN(C, 4), GPIO_OUT_LOW)
GPIO(PCH_PWROK_EC_R, PIN(D, 3), GPIO_ODR_LOW)
GPIO(SYS_PWROK_EC, PIN(F, 5), GPIO_ODR_LOW)

GPIO_INT(WP_L, PIN(D, 5), GPIO_INT_BOTH, fake_interrupt)
GPIO(EC_SPI_OE_MECC_R, PIN(A, 7), GPIO_OUT_HIGH) /* 1.8V */
GPIO(PROCHOT_EC, PIN(6, 0), GPIO_INPUT)

GPIO(BC_ACOK_EC, PIN(0, 2), GPIO_INPUT)

UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(EN_PP5000)

/* MTL-RVP has custom GPIO implementation for reading board ID */
UNIMPLEMENTED(BOARD_VERSION1)
UNIMPLEMENTED(BOARD_VERSION2)
UNIMPLEMENTED(BOARD_VERSION3)

/* Alternate pins for UART */
ALTERNATE(PIN_MASK(6, BIT(4) | BIT(5)), GPIO_ALT_FUNC_DEFAULT, MODULE_UART,
	  GPIO_FLAG_NONE)
