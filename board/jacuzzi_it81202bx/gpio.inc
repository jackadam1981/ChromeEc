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
#if 0
GPIO_INT(USB_C0_PD_INT_ODL, PIN(B, 1), GPIO_INT_FALLING | GPIO_PULL_UP,
         tcpc_alert_event)
GPIO_INT(POWER_BUTTON_L,    PIN(A, 0),  GPIO_INT_BOTH | GPIO_PULL_UP,
         power_button_interrupt) /* EC_PWR_BTN_ODL */
GPIO_INT(AP_IN_SLEEP_L,     PIN(C, 12), GPIO_INT_BOTH | GPIO_PULL_DOWN,
         power_signal_interrupt)
GPIO_INT(PMIC_EC_RESETB,    PIN(B, 7),  GPIO_INT_BOTH | GPIO_PULL_DOWN,
         power_signal_interrupt)
GPIO_INT(WARM_RESET_REQ,    PIN(A, 3),  GPIO_INT_RISING | GPIO_PULL_DOWN,
         chipset_reset_request_interrupt)
GPIO_INT_RW(ACCEL_INT_ODL,  PIN(A, 4), GPIO_INT_FALLING | GPIO_SEL_1P8V | GPIO_PULL_UP,
         bmi160_interrupt)
GPIO_INT_RO(EMMC_CMD,       PIN(B, 15), GPIO_INT_FALLING,
         emmc_cmd_interrupt)
GPIO_INT(SPI1_NSS,          PIN(A, 15), GPIO_INT_BOTH | GPIO_PULL_UP,
         spi_event) /* SPI_AP_EC_CS_L */
GPIO_INT(LID_OPEN,          PIN(C, 5),  GPIO_INT_BOTH,
         lid_interrupt)
GPIO_INT(AC_PRESENT,        PIN(A, 6),  GPIO_INT_BOTH,
         extpower_interrupt) /* ACOK_OD */
GPIO_INT(BC12_EC_INT_ODL,   PIN(C, 9),  GPIO_INT_FALLING,
         bc12_interrupt)
GPIO_INT(IT8801_SMB_INT,    PIN(A, 8),  GPIO_INT_FALLING | GPIO_PULL_UP,
         io_expander_it8801_interrupt) /* KB_INT_ODL */
GPIO_INT(AP_EC_WATCHDOG_L,  PIN(D, 2),  GPIO_INT_FALLING,
         chipset_watchdog_interrupt)
GPIO_INT(TABLET_MODE_L,     PIN(B, 11), GPIO_INT_BOTH,
         gmr_tablet_switch_isr)
#else
GPIO_INT(USB_C0_PD_INT_ODL, PIN(A, 0), GPIO_INT_FALLING | GPIO_PULL_UP,
         tcpc_alert_event)
GPIO_INT(POWER_BUTTON_L,    PIN(A, 1), GPIO_INT_BOTH | GPIO_PULL_UP,
         power_button_interrupt) /* EC_PWR_BTN_ODL */
GPIO_INT(AP_IN_SLEEP_L,     PIN(A, 2), GPIO_INT_BOTH | GPIO_PULL_DOWN,
         power_signal_interrupt)
GPIO_INT(PMIC_EC_RESETB,    PIN(A, 3), GPIO_INT_BOTH | GPIO_PULL_DOWN,
         power_signal_interrupt)
GPIO_INT(WARM_RESET_REQ,    PIN(A, 7), GPIO_INT_RISING | GPIO_PULL_DOWN,
         chipset_reset_request_interrupt)
GPIO_INT_RW(ACCEL_INT_ODL,  PIN(E, 1), GPIO_INT_FALLING | GPIO_SEL_1P8V | GPIO_PULL_UP,
         bmi160_interrupt)
GPIO_INT_RO(BOOTBLOCK_EN_L, PIN(E, 3), GPIO_INT_RISING, ap_jump_to_bl)
#endif
#if 0
GPIO_INT_RO(EMMC_CMD,       PIN(E, 2), GPIO_INT_FALLING,
         emmc_cmd_interrupt)
#endif
#if 0
GPIO_INT(SPI1_NSS,          PIN(E, 3), GPIO_INT_BOTH | GPIO_PULL_UP,
         spi_event) /* SPI_AP_EC_CS_L */
#else
GPIO_INT(SPI0_CS,           PIN(M, 5), GPIO_INT_FALLING,
         spi_event)        /* SPI slave Chip Select -- AP_SPI_EC_CS_L */
#endif
GPIO_INT(LID_OPEN,          PIN(E, 4), GPIO_INT_BOTH,
         lid_interrupt)
GPIO_INT(AC_PRESENT,        PIN(I, 0), GPIO_INT_BOTH,
         extpower_interrupt) /* ACOK_OD */
GPIO_INT(BC12_EC_INT_ODL,   PIN(I, 1), GPIO_INT_FALLING,
         bc12_interrupt)
#if 0
GPIO_INT(IT8801_SMB_INT,    PIN(I, 2), GPIO_INT_FALLING | GPIO_PULL_UP,
         io_expander_it8801_interrupt) /* KB_INT_ODL */
#endif
GPIO_INT(AP_EC_WATCHDOG_L,  PIN(I, 3), GPIO_INT_FALLING,
         chipset_watchdog_interrupt)
GPIO_INT(TABLET_MODE_L,     PIN(I, 4), GPIO_INT_BOTH,
         gmr_tablet_switch_isr)
GPIO_INT(UART1_RX,          PIN(B, 0), GPIO_INT_FALLING,
         uart_deepsleep_interrupt) /* UART_DEBUG_TX_EC_RX */

#if 0
/* Unimplemented interrupts */
GPIO(ALS_RGB_INT_ODL,      PIN(C, 10), GPIO_INPUT)

/* Reset pins */
GPIO(AP_SYS_RST_L,         PIN(C, 11), GPIO_OUT_LOW)
GPIO(PMIC_WATCHDOG_L,      PIN(A, 2),  GPIO_OUT_LOW)
GPIO(PMIC_EN_ODL,          PIN(F, 0),  GPIO_ODR_HIGH)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C1_SCL,             PIN(B, 8),  GPIO_INPUT)
GPIO(I2C1_SDA,             PIN(B, 9),  GPIO_INPUT)
GPIO(I2C2_SCL,             PIN(A, 11), GPIO_INPUT)
GPIO(I2C2_SDA,             PIN(A, 12), GPIO_INPUT)
GPIO(I2C3_SCL,             PIN(A, 5),  GPIO_ODR_HIGH)
GPIO(I2C3_SDA,             PIN(C, 4),  GPIO_ODR_HIGH)

/* Analog pins */
GPIO(BOARD_ID,             PIN(C, 0),  GPIO_ANALOG)
GPIO(EC_SKU_ID,            PIN(B, 0),  GPIO_ANALOG)

/* Other input pins */
GPIO(WP_L,                 PIN(C, 8),  GPIO_INPUT) /* EC_FLASH_WP_ODL */
GPIO(BOOT0,                PIN(F, 11), GPIO_INPUT)
GPIO(CCD_MODE_ODL,         PIN(A, 1),  GPIO_INPUT)

/* Other output pins */
GPIO(EC_BATT_PRES_ODL,     PIN(A, 7),  GPIO_INPUT)
GPIO(EC_BL_EN_OD,          PIN(A, 13), GPIO_ODR_HIGH)
GPIO(EN_USBA_5V,           PIN(C, 14), GPIO_OUT_LOW)
GPIO(EC_SENSOR_SPI_MISO,   PIN(C, 2),  GPIO_INPUT)
GPIO(EC_SENSOR_SPI_MOSI,   PIN(C, 3),  GPIO_OUT_LOW)
GPIO(EC_SENSOR_SPI_NSS,    PIN(B, 12), GPIO_OUT_HIGH)
GPIO(EC_SENSOR_SPI_CK,     PIN(B, 10), GPIO_OUT_LOW)
GPIO(ENTERING_RW,          PIN(C, 6),  GPIO_ODR_HIGH) /* EC_ENTERING_RW_ODL */
GPIO(EC_INT_L,             PIN(C, 7),  GPIO_ODR_HIGH) /* EC_AP_INT_ODL */
GPIO(EC_BOARD_ID_EN_L,     PIN(C, 15), GPIO_ODR_HIGH) /* EC_BOARD_ID_EN_ODL */
GPIO(USB_C0_HPD_OD,        PIN(F, 1),  GPIO_ODR_LOW)
GPIO(BOOTBLOCK_EN_L,       PIN(C, 1),  GPIO_ODR_HIGH)
GPIO(USB_C0_DISCHARGE,     PIN(B, 6),  GPIO_OUT_LOW)

IOEX(LED_BLUE,             EXPIN(0, 1, 4), GPIO_OUT_HIGH)      /* LED BLUE */
IOEX(LED_GREEN,            EXPIN(0, 1, 3), GPIO_OUT_HIGH)      /* LED GREEN */
IOEX(LED_ORANGE,           EXPIN(0, 1, 2), GPIO_OUT_HIGH)      /* LED ORANGE */
#else
UNIMPLEMENTED(ALS_RGB_INT_ODL)
UNIMPLEMENTED(AP_SYS_RST_L)
UNIMPLEMENTED(PMIC_WATCHDOG_L)
UNIMPLEMENTED(PMIC_EN_ODL)
GPIO(I2C_A_SCL,              PIN(B, 3), GPIO_INPUT)
GPIO(I2C_A_SDA,              PIN(B, 4), GPIO_INPUT)
GPIO(I2C_B_SCL,              PIN(C, 1), GPIO_INPUT)
GPIO(I2C_B_SDA,              PIN(C, 2), GPIO_INPUT)
GPIO(I2C_C_SCL,              PIN(F, 6), GPIO_INPUT)
GPIO(I2C_C_SDA,              PIN(F, 7), GPIO_INPUT)
GPIO(I2C_E_SCL,              PIN(A, 4), GPIO_INPUT)
GPIO(I2C_E_SDA,              PIN(A, 5), GPIO_INPUT)
GPIO(GPIOH3,                 PIN(H, 3), GPIO_OUT_LOW)
UNIMPLEMENTED(BOARD_ID)
UNIMPLEMENTED(EC_SKU_ID)
UNIMPLEMENTED(WP)
UNIMPLEMENTED(BOOT0)
UNIMPLEMENTED(CCD_MODE_ODL)
UNIMPLEMENTED(EC_BATT_PRES_ODL)
UNIMPLEMENTED(EC_BL_EN_OD)
UNIMPLEMENTED(EN_USBA_5V)
UNIMPLEMENTED(EC_SENSOR_SPI_MISO)
UNIMPLEMENTED(EC_SENSOR_SPI_MOSI)
UNIMPLEMENTED(EC_SENSOR_SPI_NSS)
UNIMPLEMENTED(EC_SENSOR_SPI_CK)
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(EC_INT_L)
UNIMPLEMENTED(EC_BOARD_ID_EN_L)
UNIMPLEMENTED(USB_C0_HPD_OD)
#if 0
UNIMPLEMENTED(BOOTBLOCK_EN_L)
#endif
UNIMPLEMENTED(USB_C0_DISCHARGE)
#if 0
IOEX(LED_BLUE,             EXPIN(0, 1, 4), GPIO_OUT_HIGH)      /* LED BLUE */
IOEX(LED_GREEN,            EXPIN(0, 1, 3), GPIO_OUT_HIGH)      /* LED GREEN */
IOEX(LED_ORANGE,           EXPIN(0, 1, 2), GPIO_OUT_HIGH)      /* LED ORANGE */
#else
UNIMPLEMENTED(LED_BLUE)
UNIMPLEMENTED(LED_GREEN)
UNIMPLEMENTED(LED_ORANGE)
#endif
#endif

#if 0
#ifdef BOARD_JACUZZI
GPIO(LID_ACCEL_INT_ODL, PIN(A, 14), GPIO_INPUT)
/* Jacuzzi doesn't have EN_PP1800_S5_L. */
UNIMPLEMENTED(EN_PP1800_S5_L)
#else
GPIO(EN_PP1800_S5_L,       PIN(A, 14), GPIO_OUT_LOW)
#endif
#else
UNIMPLEMENTED(EN_PP1800_S5_L)
#endif
/*
 * TODO(b:138352732): On IT88801 expander, To be readded once IT8801 driver and
 * gpio expander framework has landed.
 */
UNIMPLEMENTED(EN_PP5000_USBC)
UNIMPLEMENTED(PMIC_FORCE_RESET_ODL)
UNIMPLEMENTED(EN_USBC_CHARGE_L)

#if 0
/* USART1: PA9/PA10 */
ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART,       0)
/* I2C MASTER: PB8/9 */
ALTERNATE(PIN_MASK(B, 0x0300), 1, MODULE_I2C,        GPIO_ODR_HIGH )
/* I2C MASTER: PA11/12 */
ALTERNATE(PIN_MASK(A, 0x1800), 5, MODULE_I2C,        GPIO_ODR_HIGH )


/* SPI1 */
/* SPI SLAVE: PB3/4/5 */
ALTERNATE(PIN_MASK(B, 0x0038), 0, MODULE_SPI,        0)
/* SPI SLAVE CS: PA15 */
ALTERNATE(PIN_MASK(A, 0x8000), 0, MODULE_SPI,        0)

/* SPI2 */
/* Shared between slave for emmc and master for bmi160 */
/* They're mutually exclusive with gpio_config_module. */
/* EMMC SPI SLAVE: PB13/14/15 */
ALTERNATE(PIN_MASK(B, 0xE000), 0, MODULE_SPI_FLASH,  0)
/* SENSORS SPI MASTER: PB10, PB12, PC2, PC3 */
ALTERNATE(PIN_MASK(B, 0x0400), 5, MODULE_SPI_MASTER, 0)
ALTERNATE(PIN_MASK(C, 0x000C), 1, MODULE_SPI_MASTER, 0)
#else
/* Alternate functions GPIO definitions */
/* I2C */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, GPIO_PULL_UP) /* I2C A */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, GPIO_PULL_UP) /* I2C B */
ALTERNATE(PIN_MASK(F, 0xC0), 1, MODULE_I2C, GPIO_PULL_UP) /* I2C C */
/* ADC */
ALTERNATE(PIN_MASK(I, 0x60), 0, MODULE_ADC, 0)  /* ADC 5,6 */
/* UART */
ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, 0) /* EC to Servo */
/* EMMC SPI SLAVE M2:CLK, M3:CMD, M6:DATA0 */
ALTERNATE(PIN_MASK(M, 0x4C), 0, MODULE_SPI_FLASH,  0)
/* SPI */
ALTERNATE(PIN_MASK(M, 0x33), 0, MODULE_SPI, 0) /* SPI for communication */
#endif
