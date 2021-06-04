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
GPIO_INT(USB_C0_PD_INT_ODL, PIN(D, 1), GPIO_INT_FALLING | GPIO_PULL_UP,
         tcpc_alert_event)
GPIO_INT(VOLUME_UP_L,       PIN(D, 6), GPIO_INT_BOTH | GPIO_PULL_UP,
         button_interrupt) /* EC_VOLUP_BTN_ODL */
GPIO_INT(VOLUME_DOWN_L,     PIN(D, 5), GPIO_INT_BOTH | GPIO_PULL_UP,
         button_interrupt) /* EC_VOLDN_BTN_ODL */
GPIO_INT(POWER_BUTTON_L,    PIN(E, 4),  GPIO_INT_BOTH | GPIO_PULL_UP,
         power_button_interrupt) /* EC_PWR_BTN_ODL */

GPIO_INT(AP_IN_SLEEP_L,     PIN(F, 2), GPIO_INT_BOTH | GPIO_PULL_DOWN,  // TODO:NEED GPIO_SEL_1P8V?
         power_signal_interrupt)
GPIO_INT(PMIC_EC_RESETB,    PIN(F, 3),  GPIO_INT_BOTH | GPIO_PULL_DOWN, // TODO:NEED GPIO_SEL_1P8V?
         power_signal_interrupt)
GPIO_INT(WARM_RESET_REQ,    PIN(D, 3),  GPIO_INT_RISING | GPIO_PULL_DOWN,// TODO:NEED GPIO_SEL_1P8V?
         chipset_reset_request_interrupt)
GPIO_INT(AP_EC_WATCHDOG_L,  PIN(C, 7),  GPIO_INT_FALLING,               // TODO:NEED GPIO_SEL_1P8V?
         chipset_watchdog_interrupt)

GPIO_INT_RW(ACCEL_INT_ODL,  PIN(J, 2), GPIO_INT_FALLING | GPIO_SEL_1P8V | GPIO_PULL_UP, // TODO:REMOVE GPIO_PULL_UP?
         motion_interrupt)
GPIO_INT(CHARGER_INT_ODL,   PIN(D, 0), GPIO_INT_FALLING | GPIO_PULL_UP,
         rt946x_interrupt)
//GPIO_INT_RO(EMMC_CMD,       PIN(M, 3), GPIO_INT_FALLING,              //TODO: REMOVE THIS?
 //        emmc_cmd_interrupt)
GPIO_INT_RO(BOOTBLOCK_EN_L, PIN(J, 1), GPIO_INT_RISING | GPIO_SEL_1P8V,
         emmc_ap_jump_to_bl)
GPIO_INT(SPI0_CS,          PIN(M, 5), GPIO_INT_BOTH | GPIO_PULL_UP,
         spi_event)
GPIO_INT_RW(SYNC_INT,       PIN(A, 7),  GPIO_INT_RISING | GPIO_PULL_DOWN,
         sync_interrupt)
GPIO_INT(HALL_INT_L,        PIN(E, 2),  GPIO_INT_BOTH,
         lid_interrupt)
GPIO_INT(GAUGE_INT_ODL,     PIN(F, 0),  GPIO_INT_FALLING | GPIO_PULL_UP,
         gauge_interrupt)
GPIO_INT(POGO_ADC_INT_L,    PIN(I, 5),  GPIO_INT_BOTH,
         pogo_adc_interrupt)
GPIO_INT(UART1_RX,          PIN(B, 0), GPIO_INT_FALLING,
         uart_deepsleep_interrupt) /* UART_DEBUG_TX_EC_RX */  //TODO SHOULD WE NEED THIS?
/* unused */
GPIO(POGO_VBUS_PRESENT,    PIN(J, 3),  GPIO_INPUT)
/* unused after board rev 5 */
GPIO(USB_C0_DP_POLARITY,   PIN(A, 1), GPIO_INPUT)
GPIO(USB_C0_DP_OE_L,       PIN(I, 6),  GPIO_INPUT)


/* Reset pins */
GPIO(AP_SYS_RST_L,         PIN(B, 6), GPIO_OUT_LOW)
GPIO(PMIC_WATCHDOG_L,      PIN(H, 0),  GPIO_OUT_LOW) //TODO NEED GPIO_ODR_LOW | GPIO_SEL_1P8V?
GPIO(PMIC_EN_ODL,          PIN(E, 1), GPIO_ODR_HIGH)
GPIO(PMIC_FORCE_RESET_ODL, PIN(C, 6),  GPIO_ODR_HIGH)
GPIO(MT6370_RST_L,         PIN(C, 0),  GPIO_OUT_LOW)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
/* EC programming */
GPIO(I2C_E_SCL,             PIN(A, 4), GPIO_INPUT | GPIO_SEL_1P8V) //TODO NEED ALTERNATE ?
GPIO(I2C_E_SDA,             PIN(A, 5), GPIO_INPUT | GPIO_SEL_1P8V) //TODO NEED ALTERNATE ?
GPIO(I2C_C_SDL,             PIN(F, 6),  GPIO_INPUT)
GPIO(I2C_C_SDA,             PIN(F, 7),  GPIO_INPUT)
GPIO(I2C_B_SCL,             PIN(C, 1),  GPIO_INPUT | GPIO_SEL_1P8V)
GPIO(I2C_B_SDA,             PIN(C, 2),  GPIO_INPUT | GPIO_SEL_1P8V)

/* Other input pins */
GPIO(WP_L,                 PIN(I, 4),  GPIO_INPUT) /* EC_FLASH_WP_ODL */ //NEED GPIO_SEL_1P8V ?
//GPIO(BOOT0,                PIN(F, 11), GPIO_INPUT) // RELTATE TO H1 NEED REMOVE ?
GPIO(CCD_MODE_ODL,         PIN(C, 4),  GPIO_INPUT)

/* Other output pins */
GPIO(ENTERING_RW,          PIN(C, 5),  GPIO_ODR_HIGH) /* EC_ENTERING_RW_ODL */
GPIO(EC_INT_L,             PIN(E, 6), GPIO_ODR_HIGH) /* EC_AP_INT_ODL */   //NEED  GPIO_SEL_1P8V ?
GPIO(EC_BOARD_ID_EN_L,     PIN(H, 5), GPIO_ODR_HIGH) /* EC_BOARD_ID_EN_ODL */
GPIO(USB_C0_HPD_OD,        PIN(J, 0),  GPIO_ODR_LOW)
GPIO(EN_PP3300_POGO,     PIN(J, 4), GPIO_OUT_LOW)
GPIO(EN_POGO_CHARGE_L,   PIN(J, 5),  GPIO_OUT_HIGH)
GPIO(EN_USBC_CHARGE_L,   PIN(F, 1),  GPIO_OUT_LOW)
GPIO(EN_PP5000_USBC,     PIN(H, 4),  GPIO_OUT_LOW)
GPIO(BC12_DET_EN,        PIN(J, 6),  GPIO_OUT_LOW)

/* NC pins, ensure they aren't in floating state. */
GPIO(NC_GPA2,              PIN(A, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPA3,              PIN(A, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPA6,              PIN(A, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPA7,              PIN(A, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPB2,              PIN(B, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPB3,              PIN(B, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPB4,              PIN(B, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPB5,              PIN(B, 5), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPC0,              PIN(C, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPC3,              PIN(C, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPD2,              PIN(D, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPD4,              PIN(D, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPD7,              PIN(D, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPE0,              PIN(E, 0), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPE3,              PIN(E, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPE5,              PIN(E, 5), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPE7,              PIN(E, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPF4,              PIN(F, 4), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPF5,              PIN(F, 5), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPH1,              PIN(H, 1), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPH2,              PIN(H, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPH3,              PIN(H, 3), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(NC_GPJ7,              PIN(J, 7), GPIO_INPUT | GPIO_PULL_DOWN)
/*
 * ADC pins don't have internal pull-down capability,
 * so we set them as output low.
 */
GPIO(NC_GPI7,              PIN(I, 7), GPIO_OUT_LOW)
/*
 * GPG3,4,5,7 don't have internal pull-down capability,
 * so we set them as output low.
 */
GPIO(NC_GPG0,              PIN(G, 0), GPIO_INPUT | GPIO_PULL_DOWN)
/* Don't touch GPG1 and GPG2 */
GPIO(NC_GPG3,              PIN(G, 3), GPIO_OUT_LOW)

/* USART1: PB0/PB1 */
ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, 0) /* EC to Servo */
/* I2C MASTER: PF6/7 */
ALTERNATE(PIN_MASK(F, 0xC0), 1, MODULE_I2C, 0) /* I2C C */
/* I2C MASTER: PC1/2 */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, GPIO_SEL_1P8V) /* I2C B */
/* ADC */
ALTERNATE(PIN_MASK(I, 0x2F), 0, MODULE_ADC, 0)  /* ADC 0,1,2,3,5 */
/* EMMC SPI SLAVE M2:CLK, M3:CMD, M6:DATA0 */
ALTERNATE(PIN_MASK(M, 0x4C), 0, MODULE_SPI_FLASH,  0)
/* SPI */
ALTERNATE(PIN_MASK(M, 0x33), 0, MODULE_SPI, 0) /* SPI for communication */
