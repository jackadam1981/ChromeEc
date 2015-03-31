/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO(LID_OPEN,          27,	GPIO_INT_BOTH_DSLEEP,                      lid_interrupt)          /* Lid switch */
GPIO(AC_PRESENT,        30, GPIO_INT_BOTH_DSLEEP,                      extpower_interrupt)     /* BC_ACOK / EC_ACIN - to know if battery or AC connected */
GPIO(WP_L,              33, GPIO_INT_BOTH,                             switch_interrupt)       /* Write protect input */
GPIO(POWER_BUTTON_L,    35, GPIO_INT_BOTH_DSLEEP,                      power_button_interrupt) /* Power button */
GPIO(RSMRST_L_PGOOD,    63, GPIO_INT_BOTH,                             power_signal_interrupt) /* RSMRST_N_PWRGD from power logic */
GPIO(ALL_SYS_PGOOD,    130, GPIO_INT_BOTH_DSLEEP,                      power_signal_interrupt) /* ALL_SYS_PWRGD from power logic */
GPIO(PCH_SLP_S4_L,     200, GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S4# signal from PCH */
GPIO(PCH_SLP_S3_L,     206, GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S3# signal from PCH */

GPIO(NC_012,            12, GPIO_INPUT | GPIO_PULL_UP,                 NULL)                   /* NC */
GPIO(USB_ILIM_SEL,      13, GPIO_OUT_HIGH,                             NULL)                   /* USB current control */
GPIO(I2C_PORT0_SCL,     15, GPIO_ODR_HIGH,                             NULL)
GPIO(I2C_PORT0_SDA,     16, GPIO_ODR_HIGH,                             NULL)

GPIO(I2C_PORT1_SCL,     22, GPIO_ODR_HIGH,                             NULL)
GPIO(I2C_PORT1_SDA,     23, GPIO_ODR_HIGH,                             NULL)
GPIO(PCH_SCI_L,         26, GPIO_ODR_HIGH,                             NULL)                   /* SCI output */

GPIO(VOLUME_UP,         31, GPIO_INT_FALLING,                          NULL)                   /* Volume up button */
GPIO(VOLUME_DOWN,       34, GPIO_INT_FALLING,                          NULL)                   /* Volume down button */
GPIO(USB2_PWR_EN,       36, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for USB2 Port */

GPIO(ENTERING_RW,       41, GPIO_OUT_LOW,                              NULL)                   /* Indicate when EC is entering RW code */
GPIO(PCH_SMI_L,         44, GPIO_ODR_HIGH,                             NULL)                   /* SMI output */
GPIO(USB_OC1_L,         45, GPIO_INT_FALLING,                          NULL)                   /* DB2 BC1.2 over current signal to EC */
GPIO(DP_USB_C_HPD_Q,    46, GPIO_OUT_HIGH,                             NULL)                   /* DP hot plug detect from EC to SOC */
GPIO(VBATA_VR_HOT_N,    47, GPIO_INT_FALLING,                          NULL)                   /* PROCHOT signal of VBATA VR connected to EC */

GPIO(VNN_I2C3_ALERT_N,  50, GPIO_INT_FALLING,                          NULL)                   /* Alert signal from VNN VR to EC */
GPIO(PCH_SUS_STAT_L,    51, GPIO_INT_FALLING,                          NULL)                   /* Signal to inform EC that SOC is entering low power state */
GPIO(TOUCHPANEL_PWREN,  52, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for Touch Panel */
GPIO(TRACKPAD_PWREN,    53, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for Track Pad */
GPIO(USB_OC0_L,         55, GPIO_INT_FALLING,                          NULL)                   /* Over current signal of the BC1.2 charger to EC */
GPIO(EC_ADC0,           56, GPIO_ANALOG,                               NULL)                   /* EC_ADC0 */
GPIO(EC_ADC1,           57, GPIO_ANALOG,                               NULL)                   /* EC_ADC1 */

GPIO(CHGR_PMON,         60, GPIO_ANALOG,                               NULL)
GPIO(WIFI_PWREN,        61, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for WiFi */
GPIO(BATT_EN_L,         62, GPIO_INPUT,                                NULL)                   /* Will be NC */
GPIO(SW_OPEN_EC,        64, GPIO_INT_BOTH,                             NULL)                   /* Signal from USB Charger to EC to indicate if the isolation switch is open or close */
GPIO(PCH_SYS_PWROK,     65, GPIO_OUT_LOW,                              NULL)                   /* EC thinks everything is up and ready (DELAY_ALL_SYS_PWRGD) */
GPIO(PCH_WAKE_L,        66, GPIO_ODR_HIGH,                             NULL)                   /* PCH wake pin */
GPIO(USB3_PWR_EN,       67, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for USB3 Port */

GPIO(USB_CTL1,         105, GPIO_OUT_HIGH,                             NULL)                   /* USB charging mode control */

GPIO(PCH_RCIN_L,       110, GPIO_ODR_HIGH,                             NULL)                   /* Reset line to PCH (for 8042 emulation) */
GPIO(NC_115,           115, GPIO_INPUT | GPIO_PULL_UP,                 NULL)                   /* NC */

GPIO(USB_PD_EC_INT,    122, GPIO_INT_BOTH,                             NULL)                   /* Interrupt from USB PD Controller to EC */
GPIO(EC_WAKE_L,        123, GPIO_INT_BOTH_DSLEEP,                      NULL)
GPIO(USB_CHG_DET_EC,   124, GPIO_INT_BOTH,                             NULL)                   /* Signal from USB Charger to EC to indicate charger has been detected */
GPIO(GYRO_INT2,        127, GPIO_INT_FALLING,                          NULL)                   /* Gyro sensor interrupt 2 to EC */

GPIO(EC_PLUG_DETECT,   132, GPIO_INT_BOTH,                             NULL)
GPIO(BAT_LED0_L,       133, GPIO_ODR_HIGH,                             NULL)                   /* Battery charging LED - blue */
GPIO(WWAN_PWREN,       135, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for WWAN - PROBE_DETECT_L */
GPIO(BAT_LED1_L,       136, GPIO_ODR_HIGH,                             NULL)                   /* Battery charging LED - orange */

GPIO(THERMAL_PROBE_EN_L,140, GPIO_OUT_HIGH,                             NULL)
GPIO(PWR_LED0_L,        141, GPIO_ODR_HIGH,                             NULL)                   /* Power LED - blue */
GPIO(PCH_RSMRST_L,      143, GPIO_OUT_LOW,                              NULL)                   /* RSMRST_N to PCH */
GPIO(PWR_LED1_L,        145, GPIO_ODR_HIGH,                             NULL)                   /* Power LED - orange */
GPIO(PVT_CS0,           146, GPIO_ODR_HIGH,                             NULL)                   /* SPI PVT Chip select */
GPIO(ALS_INT,           147, GPIO_INT_FALLING,                          NULL)                   /* ALS sensor interrupt to EC */

GPIO(WLAN_OFF_L,        150, GPIO_ODR_HIGH,                             NULL)                   /* Wireless LAN */
GPIO(CPU_PROCHOT,       151, GPIO_OUT_LOW,                              NULL)
GPIO(KBD_IRQ_L,         152, GPIO_ODR_HIGH,                             NULL)                   /* Negative edge triggered irq. */
GPIO(NC_154,            154, GPIO_INPUT | GPIO_PULL_UP,                 NULL)                   /* NC */
GPIO(CORE_PWROK,        155, GPIO_ODR_HIGH,                             NULL)                   /* CORE_PWR_OK_R */
GPIO(LID_OPEN2,         156, GPIO_INT_BOTH_DSLEEP,                      NULL)                   /* LID_OPEN_OUT2_R */
GPIO(PCH_SUSPWRDNACK,   157, GPIO_INT_FALLING,                          NULL)                   /* PMC SUSPWRDNACK signal from SOC to EC */

GPIO(PCH_PWRBTN_L,      160, GPIO_OUT_HIGH,                             NULL)                   /* Power button output to PCH */
GPIO(GYRO_INT1,         161, GPIO_INT_FALLING,                          NULL)                   /* Gyro sensor interrupt 1 to EC */
GPIO(MUX_CROSS_BAR_EN,  163, GPIO_OUT_LOW,                              NULL)                   /* Enable signal of the USB Type C Mux */

GPIO(MUX_CROSS_BAR_POL, 201, GPIO_OUT_LOW,                              NULL)                   /* Output from EC to POL signal of USB Type C Mux */
GPIO(EC_BL_DISABLE_L,   202, GPIO_OUT_HIGH,                             NULL)                   /* EDP backligh disable signal from EC */
GPIO(SMC_SHUTDOWN,      203, GPIO_OUT_LOW,                              NULL)                   /* Shutdown signal from EC to power sequencing PLD */
GPIO(MUX_CROSS_BAR_MODE,204, GPIO_OUT_LOW,                              NULL)                   /* Output from EC - Type C Mux mode select */

GPIO(SUSPWRDNACK_SOC_EC,210, GPIO_OUT_LOW,                              NULL)                   /* SUSPWRDNACK signal from EC to MOIC device */
GPIO(PCH_SLP_SX_L,      211, GPIO_INT_BOTH_DSLEEP,                      NULL)                   /* Sleep SOIX signal from SOC to EC */

/* Alternate functions GPIO definition */
ALTERNATE(16,   0x24,   1,      MODULE_UART,            0)                      /* UART0 */

ALTERNATE(1,    0x60,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C0: Battery Charger */
ALTERNATE(2,    0x3f,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C1: Temp Sensor / I2C2: SOC / I2C3: VNN */
/* PORT SEL [3:0] has to be set to 1 to access this port 1 of controller 0 */
ALTERNATE(13,   0x10,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C0 - Port 1 : Accel SCL */
ALTERNATE(1,    0x80,   2,      MODULE_I2C,             GPIO_PULL_UP)           /* I2C0 - Port 1 : Accel SDA */

ALTERNATE(0,    0xfc,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT)
ALTERNATE(1,    0x03,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT)
ALTERNATE(10,   0xd8,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT)
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
 GPIO(KBD_KSO2,         1,      GPIO_KB_OUTPUT_COL2,    NULL)                   /* Negative edge triggered irq. */
#else
 ALTERNATE(0,   0x02,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_OUTPUT_COL2)
#endif
ALTERNATE(3,    0x04,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)
ALTERNATE(4,    0x0d,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)
ALTERNATE(12,   0x60,   2,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)
ALTERNATE(14,   0x14,   3,      MODULE_KEYBOARD_SCAN,   GPIO_KB_INPUT)

ALTERNATE(1,    0x10,   1,      MODULE_LPC,             0)                      /* 14: CLKRUN# */
ALTERNATE(11,   0x9e,   1,      MODULE_LPC,             0)                      /* 111~114: LAD[0:3], 117: PCI_CLK */
ALTERNATE(11,   0x40,   1,      MODULE_LPC,             GPIO_INT_BOTH)          /* 116: LRESET# */
ALTERNATE(12,   0x01,   1,      MODULE_LPC,             0)                      /* 120: LFRAME# */

ALTERNATE(5,    0x10,   1,      MODULE_SPI,             0)                      /* 54: MOSI */
ALTERNATE(16,   0x10,   1,      MODULE_SPI,             0)                      /* 164: MISO */
ALTERNATE(15,   0x08,   1,      MODULE_SPI,             0)                      /* 153: CLK */
