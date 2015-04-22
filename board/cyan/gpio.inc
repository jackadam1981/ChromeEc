/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO_INT(LID_OPEN,          PIN(27), GPIO_INT_BOTH_DSLEEP,                      lid_interrupt)          /* Lid switch */
GPIO_INT(AC_PRESENT,        PIN(30), GPIO_INT_BOTH_DSLEEP,                      extpower_interrupt)     /* BC_ACOK / EC_ACIN - to know if battery or AC connected */
GPIO_INT(WP_L,              PIN(33), GPIO_INT_BOTH,                             switch_interrupt)       /* Write protect input */
GPIO_INT(POWER_BUTTON_L,    PIN(35), GPIO_INT_BOTH_DSLEEP,                      power_button_interrupt) /* Power button */
GPIO_INT(RSMRST_L_PGOOD,    PIN(63), GPIO_INT_BOTH,                             power_signal_interrupt) /* RSMRST_N_PWRGD from power logic */
GPIO_INT(ALL_SYS_PGOOD,     PIN(130), GPIO_INT_BOTH_DSLEEP,                      power_signal_interrupt) /* ALL_SYS_PWRGD from power logic */
GPIO_INT(PCH_SLP_S4_L,      PIN(200), GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S4# signal from PCH */
GPIO_INT(PCH_SLP_S3_L,      PIN(206), GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S3# signal from PCH */

GPIO(NC_012,            PIN(12), GPIO_INPUT | GPIO_PULL_UP)       /* NC */
GPIO(USB_ILIM_SEL,      PIN(13), GPIO_OUT_HIGH)                   /* USB current control */
GPIO(I2C_PORT0_SCL,     PIN(15), GPIO_ODR_HIGH)
GPIO(I2C_PORT0_SDA,     PIN(16), GPIO_ODR_HIGH)

GPIO(I2C_PORT1_SCL,     PIN(22), GPIO_ODR_HIGH)
GPIO(I2C_PORT1_SDA,     PIN(23), GPIO_ODR_HIGH)
GPIO(PCH_SCI_L,         PIN(26), GPIO_ODR_HIGH)                   /* SCI output */

GPIO(VOLUME_UP,         PIN(31), GPIO_INT_FALLING)                /* Volume up button */
GPIO(VOLUME_DOWN,       PIN(34), GPIO_INT_FALLING)                /* Volume down button */
GPIO(USB2_PWR_EN,       PIN(36), GPIO_OUT_HIGH)                   /* Enable power for USB2 Port */

GPIO(ENTERING_RW,       PIN(41), GPIO_OUT_LOW)                    /* Indicate when EC is entering RW code */
GPIO(PCH_SMI_L,         PIN(44), GPIO_ODR_HIGH)                   /* SMI output */
GPIO(USB_OC1_L,         PIN(45), GPIO_INT_FALLING)                /* DB2 BC1.2 over current signal to EC */
GPIO(DP_USB_C_HPD_Q,    PIN(46), GPIO_OUT_HIGH)                   /* DP hot plug detect from EC to SOC */
GPIO(VBATA_VR_HOT_N,    PIN(47), GPIO_INT_FALLING)                /* PROCHOT signal of VBATA VR connected to EC */

GPIO(VNN_I2C3_ALERT_N,  PIN(50), GPIO_INT_FALLING)                /* Alert signal from VNN VR to EC */
GPIO(PCH_SUS_STAT_L,    PIN(51), GPIO_INT_FALLING)                /* Signal to inform EC that SOC is entering low power state */
GPIO(TOUCHPANEL_PWREN,  PIN(52), GPIO_OUT_HIGH)                   /* Enable power for Touch Panel */
GPIO(TRACKPAD_PWREN,    PIN(53), GPIO_OUT_HIGH)                   /* Enable power for Track Pad */
GPIO(USB_OC0_L,         PIN(55), GPIO_INT_FALLING)                /* Over current signal of the BC1.2 charger to EC */
GPIO(EC_ADC0,           PIN(56), GPIO_ANALOG)                     /* EC_ADC0 */
GPIO(EC_ADC1,           PIN(57), GPIO_ANALOG)                     /* EC_ADC1 */

GPIO(CHGR_PMON,         PIN(60), GPIO_ANALOG)
GPIO(WIFI_PWREN,        PIN(61), GPIO_OUT_HIGH)                   /* Enable power for WiFi */
GPIO(BATT_EN_L,         PIN(62), GPIO_INPUT)                      /* Will be NC */
GPIO(SW_OPEN_EC,        PIN(64), GPIO_INT_BOTH)                   /* Signal from USB Charger to EC to indicate if the isolation switch is open or close */
GPIO(PCH_SYS_PWROK,     PIN(65), GPIO_OUT_LOW)                    /* EC thinks everything is up and ready (DELAY_ALL_SYS_PWRGD) */
GPIO(PCH_WAKE_L,        PIN(66), GPIO_ODR_HIGH)                   /* PCH wake pin */
GPIO(USB3_PWR_EN,       PIN(67), GPIO_OUT_HIGH)                   /* Enable power for USB3 Port */

GPIO(USB_CTL1,          PIN(105), GPIO_OUT_HIGH)                   /* USB charging mode control */

GPIO(PCH_RCIN_L,        PIN(110), GPIO_ODR_HIGH)                   /* Reset line to PCH (for 8042 emulation) */
GPIO(NC_115,            PIN(115), GPIO_INPUT | GPIO_PULL_UP)       /* NC */

GPIO(USB_PD_EC_INT,     PIN(122), GPIO_INT_BOTH)                   /* Interrupt from USB PD Controller to EC */
GPIO(EC_WAKE_L,         PIN(123), GPIO_INT_BOTH_DSLEEP)
GPIO(USB_CHG_DET_EC,    PIN(124), GPIO_INT_BOTH)                   /* Signal from USB Charger to EC to indicate charger has been detected */
GPIO(GYRO_INT2,         PIN(127), GPIO_INT_FALLING)                /* Gyro sensor interrupt 2 to EC */

GPIO(EC_PLUG_DETECT,    PIN(132), GPIO_INT_BOTH)
GPIO(BAT_LED0_L,        PIN(133), GPIO_ODR_HIGH)                   /* Battery charging LED - blue */
GPIO(WWAN_PWREN,        PIN(135), GPIO_OUT_HIGH)                   /* Enable power for WWAN - PROBE_DETECT_L */
GPIO(BAT_LED1_L,        PIN(136), GPIO_ODR_HIGH)                   /* Battery charging LED - orange */

GPIO(THERMAL_PROBE_EN_L,PIN(140), GPIO_OUT_HIGH)
GPIO(PWR_LED0_L,        PIN(141), GPIO_ODR_HIGH)                   /* Power LED - blue */
GPIO(PCH_RSMRST_L,      PIN(143), GPIO_OUT_LOW)                    /* RSMRST_N to PCH */
GPIO(PWR_LED1_L,        PIN(145), GPIO_ODR_HIGH)                   /* Power LED - orange */
GPIO(PVT_CS0,           PIN(146), GPIO_ODR_HIGH)                   /* SPI PVT Chip select */
GPIO(ALS_INT,           PIN(147), GPIO_INT_FALLING)                /* ALS sensor interrupt to EC */

GPIO(WLAN_OFF_L,        PIN(150), GPIO_ODR_HIGH)                   /* Wireless LAN */
GPIO(CPU_PROCHOT,       PIN(151), GPIO_OUT_LOW)
GPIO(KBD_IRQ_L,         PIN(152), GPIO_ODR_HIGH)                   /* Negative edge triggered irq. */
GPIO(NC_154,            PIN(154), GPIO_INPUT | GPIO_PULL_UP)       /* NC */
GPIO(CORE_PWROK,        PIN(155), GPIO_ODR_HIGH)                   /* CORE_PWR_OK_R */
GPIO(LID_OPEN2,         PIN(156), GPIO_INT_BOTH_DSLEEP)            /* LID_OPEN_OUT2_R */
GPIO(PCH_SUSPWRDNACK,   PIN(157), GPIO_INT_FALLING)                /* PMC SUSPWRDNACK signal from SOC to EC */

GPIO(PCH_PWRBTN_L,      PIN(160), GPIO_OUT_HIGH)                   /* Power button output to PCH */
GPIO(GYRO_INT1,         PIN(161), GPIO_INT_FALLING)                /* Gyro sensor interrupt 1 to EC */
GPIO(MUX_CROSS_BAR_EN,  PIN(163), GPIO_OUT_LOW)                    /* Enable signal of the USB Type C Mux */

GPIO(MUX_CROSS_BAR_POL, PIN(201), GPIO_OUT_LOW)                    /* Output from EC to POL signal of USB Type C Mux */
GPIO(EC_BL_DISABLE_L,   PIN(202), GPIO_OUT_HIGH)                   /* EDP backligh disable signal from EC */
GPIO(SMC_SHUTDOWN,      PIN(203), GPIO_OUT_LOW)                    /* Shutdown signal from EC to power sequencing PLD */
GPIO(MUX_CROSS_BAR_MODE,PIN(204), GPIO_OUT_LOW)                    /* Output from EC - Type C Mux mode select */

GPIO(SUSPWRDNACK_SOC_EC,PIN(210), GPIO_OUT_LOW)                    /* SUSPWRDNACK signal from EC to MOIC device */
GPIO(PCH_SLP_SX_L,      PIN(211), GPIO_INT_BOTH_DSLEEP)            /* Sleep SOIX signal from SOC to EC */

/* Alternate functions GPIO definition */
ALTERNATE(PORT(16),     0x24,   1,      MODULE_UART,            0)                      /* UART0 */

ALTERNATE(PORT(1),      0x60,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C0: Battery Charger */
ALTERNATE(PORT(2),      0x3f,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C1: Temp Sensor / I2C2: SOC / I2C3: VNN */
/* PORT SEL [3:0] has to be set to 1 to access this port 1 of controller 0 */
ALTERNATE(PORT(13),     0x10,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C0 - Port 1 : Accel SCL */
ALTERNATE(PORT(1),      0x80,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C0 - Port 1 : Accel SDA */

ALTERNATE(PORT(0),      0xfc,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT)
ALTERNATE(PORT(1),      0x03,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT)
ALTERNATE(PORT(10)   ,  0xd8,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT)
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
GPIO(KBD_KSO2,         PIN(1),      GPIO_KB_OUTPUT_COL2)                   /* Negative edge triggered irq. */
#else
 ALTERNATE(PORT(0),     0x02,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT_COL2)
#endif
ALTERNATE(PORT(3),      0x04,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)
ALTERNATE(PORT(4),      0x0d,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)
ALTERNATE(PORT(12),     0x60,   2,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)
ALTERNATE(PORT(14),     0x14,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)

ALTERNATE(PORT(1),      0x10,   1,      MODULE_LPC,             0)                      /* 14: CLKRUN# */
ALTERNATE(PORT(11),     0x9e,   1,      MODULE_LPC,             0)                      /* 111~114: LAD[0:3], 117: PCI_CLK */
ALTERNATE(PORT(11),     0x40,   1,      MODULE_LPC,             GPIO_INT_BOTH)          /* 116: LRESET# */
ALTERNATE(PORT(12),     0x01,   1,      MODULE_LPC,             0)                      /* 120: LFRAME# */

ALTERNATE(PORT(5),      0x10,   1,      MODULE_SPI,             0)                      /* 54: MOSI */
ALTERNATE(PORT(16),     0x10,   1,      MODULE_SPI,             0)                      /* 164: MISO */
ALTERNATE(PORT(15),     0x08,   1,      MODULE_SPI,             0)                      /* 153: CLK */
