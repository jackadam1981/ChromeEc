/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first.
 */

/* Interrupts */
GPIO_INT(USB_C0_PD_INT_ODL, PIN(D, 1), GPIO_INT_FALLING | GPIO_SEL_1P8V,
         tcpc_alert_event)
GPIO_INT(POWER_BUTTON_L,    PIN(E, 4), GPIO_INT_BOTH | GPIO_SEL_1P8V,
         power_button_interrupt) /* EC_PWR_BTN_ODL */
GPIO_INT(AP_IN_SLEEP_L,     PIN(F, 2), GPIO_INT_BOTH | GPIO_PULL_DOWN | GPIO_SEL_1P8V,
         power_signal_interrupt)
GPIO_INT(PMIC_EC_RESETB,    PIN(F, 3), GPIO_INT_BOTH | GPIO_PULL_DOWN | GPIO_SEL_1P8V,
         power_signal_interrupt)
GPIO_INT(WARM_RESET_REQ,    PIN(D, 3), GPIO_INT_RISING | GPIO_PULL_DOWN | GPIO_SEL_1P8V,
         chipset_reset_request_interrupt)
GPIO_INT_RW(ACCEL_INT_ODL,  PIN(J, 2), GPIO_INT_FALLING | GPIO_SEL_1P8V,
         bmi160_interrupt)
GPIO_INT_RO(BOOTBLOCK_EN_L, PIN(J, 1), GPIO_INT_RISING | GPIO_SEL_1P8V,
         emmc_ap_jump_to_bl)
GPIO_INT(SPI0_CS,           PIN(M, 5), GPIO_INT_FALLING,
         spi_event)        /* SPI slave Chip Select -- AP_SPI_EC_CS_L */
GPIO_INT(LID_OPEN,          PIN(E, 2), GPIO_INT_BOTH,
         lid_interrupt)
GPIO_INT(AC_PRESENT,        PIN(E, 5), GPIO_INT_BOTH,
         extpower_interrupt) /* ACOK_OD */
GPIO_INT(BC12_EC_INT_ODL,   PIN(J, 6), GPIO_INT_FALLING | GPIO_SEL_1P8V,
         bc12_interrupt)
GPIO_INT(AP_EC_WATCHDOG_L,  PIN(C, 7), GPIO_INT_FALLING | GPIO_SEL_1P8V,
         chipset_watchdog_interrupt)
GPIO_INT(TABLET_MODE_L,     PIN(J, 7), GPIO_INT_BOTH,
         gmr_tablet_switch_isr)
GPIO_INT(UART1_RX,          PIN(B, 0), GPIO_INT_FALLING,
         uart_deepsleep_interrupt) /* UART_DEBUG_TX_EC_RX */
/* Unimplemented interrupts */
GPIO(VOLUME_DOWN_L,         PIN(D, 5), GPIO_INPUT | GPIO_SEL_1P8V)
GPIO(VOLUME_UP_L,           PIN(D, 6), GPIO_INPUT | GPIO_SEL_1P8V)
GPIO(ALS_RGB_INT_ODL,       PIN(F, 0), GPIO_INPUT)

/* Reset pins */
GPIO(AP_SYS_RST_L,          PIN(B, 6), GPIO_OUT_LOW)
/* 1.8V PP or 1.8V OD output with external 10K PU  */
GPIO(PMIC_WATCHDOG_L,       PIN(H, 0), GPIO_ODR_LOW | GPIO_SEL_1P8V)
/* OD output with 5VT (there is 5V internal PU on PWRKEY of MT6358) */
GPIO(PMIC_EN_ODL,           PIN(E, 1), GPIO_ODR_HIGH)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
/* EC programming */
GPIO(I2C_E_SCL,              PIN(A, 4), GPIO_INPUT)
GPIO(I2C_E_SDA,              PIN(A, 5), GPIO_INPUT)
/* battery and charger */
GPIO(I2C_A_SCL,              PIN(B, 3), GPIO_INPUT)
GPIO(I2C_A_SDA,              PIN(B, 4), GPIO_INPUT)
/* sensor */
GPIO(I2C_B_SCL,              PIN(C, 1), GPIO_INPUT)
GPIO(I2C_B_SDA,              PIN(C, 2), GPIO_INPUT)
/* typec */
GPIO(I2C_C_SCL,              PIN(F, 6), GPIO_INPUT)
GPIO(I2C_C_SDA,              PIN(F, 7), GPIO_INPUT)

/* Other input pins */
/* TODO(WP_L): change to interrupt pin ? */
GPIO(WP_L,                 PIN(I, 4),  GPIO_INPUT | GPIO_SEL_1P8V) /* EC_FLASH_WP_ODL */
GPIO(CCD_MODE_ODL,         PIN(C, 4),  GPIO_INPUT)

/* Other output pins */
GPIO(EC_BATT_PRES_ODL,     PIN(C, 0), GPIO_INPUT)
GPIO(EC_BL_EN_OD,          PIN(B, 5), GPIO_ODR_HIGH | GPIO_SEL_1P8V)
GPIO(EN_USBA_5V,           PIN(B, 7), GPIO_OUT_LOW)
UNIMPLEMENTED(EC_SENSOR_SPI_NSS)
GPIO(ENTERING_RW,          PIN(C, 5), GPIO_ODR_HIGH)  /* EC_ENTERING_RW_ODL */
GPIO(EC_INT_L,             PIN(E, 6), GPIO_ODR_HIGH | GPIO_SEL_1P8V)  /* EC_AP_INT_ODL */
GPIO(EC_BOARD_ID_EN_L,     PIN(H, 5), GPIO_ODR_HIGH)  /* EC_BOARD_ID_EN_ODL */
GPIO(USB_C0_HPD_OD,        PIN(J, 0), GPIO_ODR_LOW)
GPIO(USB_C0_DISCHARGE,     PIN(H, 3), GPIO_OUT_LOW)

/* LEDs */
GPIO(LED_BLUE,             PIN(A, 2), GPIO_OUT_HIGH)
GPIO(LED_GREEN,            PIN(A, 1), GPIO_OUT_HIGH)
GPIO(LED_ORANGE,           PIN(A, 0), GPIO_OUT_HIGH)

GPIO(EN_PP1800_S5_L,       PIN(D, 0), GPIO_ODR_LOW | GPIO_SEL_1P8V)

GPIO(EN_PP5000_USBC,       PIN(D, 4), GPIO_INPUT)
GPIO(PMIC_FORCE_RESET_ODL, PIN(C, 6), GPIO_INPUT)
GPIO(EN_USBC_CHARGE_L,     PIN(F, 1), GPIO_INPUT)

/* Alternate functions GPIO definitions */
/* I2C */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, 0) /* I2C A */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, GPIO_SEL_1P8V) /* I2C B */
ALTERNATE(PIN_MASK(F, 0xC0), 1, MODULE_I2C, GPIO_SEL_1P8V) /* I2C C */
/* ADC */
ALTERNATE(PIN_MASK(I, 0x06), 0, MODULE_ADC, 0)  /* ADC 1,2 */
/* UART */
ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, 0) /* EC to Servo */
/* EMMC SPI SLAVE M2:CLK, M3:CMD, M6:DATA0 */
ALTERNATE(PIN_MASK(M, 0x4C), 0, MODULE_SPI_FLASH,  0)
/* SPI */
ALTERNATE(PIN_MASK(M, 0x33), 0, MODULE_SPI, 0) /* SPI for communication */
