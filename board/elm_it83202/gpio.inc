/* -*- mode:c -*-
 *
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(AC_PRESENT,     PIN(E, 1), GPIO_INT_BOTH,    extpower_interrupt)
GPIO_INT(LID_OPEN,       PIN(E, 2), GPIO_INT_BOTH,    lid_interrupt)          /* LID switch detection */
GPIO_INT(SUSPEND_L,      PIN(E, 3), GPIO_INT_BOTH,    power_signal_interrupt) /* AP suspend/resume state */
GPIO_INT(SOC_POWER_GOOD, PIN(F, 0), GPIO_INT_BOTH,    power_signal_interrupt)
GPIO_INT(PD_MCU_INT,     PIN(F, 1), GPIO_INT_RISING,  pd_mcu_interrupt)       /* Signal from PD MCU, external pull-up */
GPIO_INT(BC12_CABLE_INT, PIN(F, 2), GPIO_INT_FALLING | GPIO_PULL_UP, usb_evt) /* interrupt from BC12 and CABLE_DET */
GPIO_INT(POWER_BUTTON_L, PIN(E, 4), GPIO_INT_BOTH    | GPIO_PULL_UP, power_button_interrupt)
GPIO_INT(SPI0_CS,       PIN(O, 1),  GPIO_INT_FALLING | GPIO_PULL_UP,  spi_event)              /* SPI Chip Select */
#ifdef CONFIG_LOW_POWER_IDLE
GPIO_INT(UART1_RX,       PIN(B, 0), GPIO_INT_FALLING, uart_deepsleep_interrupt) /* UART1 RX input */
#endif

/* sensor power control */
GPIO(SENSOR_PWR_EN_L,    PIN(A, 0), GPIO_OUT_LOW)
/* Inputs without interrupt handlers */
GPIO(5V_POWER_GOOD,      PIN(A, 1), GPIO_INPUT)
GPIO(EC_WAKE,            PIN(A, 2), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(WP_L,               PIN(A, 3), GPIO_INPUT)   /* Write protect input */
/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C0_SCL,           PIN(B, 3), GPIO_INPUT)    /* EC I2C */
GPIO(I2C0_SDA,           PIN(B, 4), GPIO_INPUT)
GPIO(BAT_PRESENT_L,      PIN(B, 5), GPIO_INPUT | GPIO_PULL_UP)
GPIO(USB_C0_VBUS_WAKE_L, PIN(B, 6), GPIO_INPUT)
GPIO(EC_INT_L,           PIN(A, 7), GPIO_ODR_HIGH)

GPIO(I2C1_SCL,           PIN(C, 1), GPIO_INPUT)    /* PD I2C */
GPIO(I2C1_SDA,           PIN(C, 2), GPIO_INPUT)
/* Outputs */
GPIO(BAT_LED0,           PIN(C, 4), GPIO_OUT_LOW)  /* LED_BLUE */
GPIO(BAT_LED1,           PIN(C, 0), GPIO_OUT_LOW)  /* LED_ORANGE */
GPIO(PWR_LED0,           PIN(C, 7), GPIO_OUT_LOW)  /* LED_BLUE */
GPIO(PWR_LED1,           PIN(C, 6), GPIO_OUT_LOW)  /* LED_ORANGE */

GPIO(EC_BL_OVERRIDE,     PIN(D, 0), GPIO_OUT_LOW)
GPIO(ENTERING_RW,        PIN(D, 1), GPIO_OUT_LOW)
GPIO(AP_RESET_L,         PIN(D, 3), GPIO_ODR_HIGH) /* Connect to the PMU_SYSRSTB */
GPIO(BC12_WAKE_L,        PIN(D, 4), GPIO_INPUT)
GPIO(USB_C0_CABLE_DET_L, PIN(D, 6), GPIO_INPUT | GPIO_PULL_UP)

GPIO(SYSTEM_POWER_H,     PIN(E, 5), GPIO_OUT_LOW)
GPIO(PMIC_PWRON_H,       PIN(E, 6), GPIO_OUT_LOW)

GPIO(PMIC_WARM_RESET_H,  PIN(F, 3), GPIO_OUT_LOW)
GPIO(LEVEL_SHIFT_EN_L,   PIN(F, 6), GPIO_OUT_LOW)  /* LID/AC level shift */
GPIO(USB_C0_5V_EN,       PIN(F, 7), GPIO_OUT_LOW)  /* USBC port 0 5V */

GPIO(USB_C0_CHARGE_L,    PIN(J, 0), GPIO_OUT_LOW)  /* USBC port 0 charge */
GPIO(USB_C0_RST,         PIN(J, 1), GPIO_ODR_HIGH) /* ANX7688 reset */
GPIO(USB_C0_PWR_EN_L,    PIN(J, 2), GPIO_ODR_HIGH) /* ANX7688 power enable */
GPIO(USB_C0_EXTPWR_EN,   PIN(J, 3), GPIO_OUT_LOW)  /* ANX7688 3.3V ext power enable */
GPIO(USB_DP_HPD,         PIN(J, 4), GPIO_INPUT)
GPIO(EN_TP_INT_L,        PIN(J, 5), GPIO_OUT_LOW)  /* touchpad interrupt enable */

/* Board version */
GPIO(BOARD_VERSION1,     PIN(I, 4), GPIO_INPUT)    /* Board ID 0 */
GPIO(BOARD_VERSION2,     PIN(I, 5), GPIO_INPUT)    /* Board ID 1 */
/* Forec board ID = 3 */
GPIO(BOARD_VERSION3,     PIN(I, 6), GPIO_INPUT | GPIO_PULL_DOWN)    /* Board ID 2 */
GPIO(BOARD_VERSION4,     PIN(I, 7), GPIO_INPUT | GPIO_PULL_DOWN)    /* Board ID 3 */

ALTERNATE(PIN_MASK(B, 0x03), 1, MODULE_UART, GPIO_PULL_UP) /* UART1 */
ALTERNATE(PIN_MASK(B, 0x18), 1, MODULE_I2C, 0)             /* I2C A SCL/SDA */
ALTERNATE(PIN_MASK(C, 0x06), 1, MODULE_I2C, 0)             /* I2C B SCL/SDA */
/* Analog pins */
ALTERNATE(PIN_MASK(I, 0x07), 1, MODULE_ADC, 0)             /* ADC 0/1/2 */
ALTERNATE(PIN_MASK(O, 0x0F), 0, MODULE_SPI,        0)      /* SPI SLAVE */
ALTERNATE(PIN_MASK(A, 0x40), 3, MODULE_SPI_MASTER, 0)      /* SSCK of SPI */
ALTERNATE(PIN_MASK(C, 0x28), 3, MODULE_SPI_MASTER, 0)      /* SMOSI/SMISO of SPI */
ALTERNATE(PIN_MASK(G, 0x01), 3, MODULE_SPI_MASTER, 0)      /* SSCE1# of SPI */
ALTERNATE(PIN_MASK(G, 0x04), 3, MODULE_SPI_MASTER, 0)      /* SSCE0# of SPI */
