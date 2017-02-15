/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * glados-mec1701 board GPIO pins
 * !!!!!!!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * MEC1701 and MEC1322 data sheets GPIO numbers are OCTAL.
 * Original glados MEC1322 used these octal numbers as base 10.
 * mec1701 and its boards will use OCTAL therefore make sure all
 * numbers used below are written as C OCTAL with a leading 0.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * TODO was 144 pin MEC1322 used in orignal glados board?
 * Many pins do not match.
 * Example: GPIO027 for LID_OPEN is not multiplexed on MEC1322.
 * On MEC1701 GPIO027 is multiplexed with KSI4.
 *
 */
/* NOTE: MEC1701H do not use these pins!!!
 * GPIO_0145 is JTAG_TDI
 * GPIO_0146 is JTAG_TDO
 * GPIO_0147 is JTAG_CLK
 * GPIO_0150 is JTAG_TMS
*/

/* Only needed if MEC1701H CONFIG_ESPI is not set, using LPC interface to PCH */
GPIO_INT(PCH_PLTRST_L,      PIN(064), GPIO_INT_BOTH | GPIO_PULL_UP, lpcrst_interrupt)

GPIO_INT(LID_OPEN,          PIN(015), GPIO_INT_BOTH | GPIO_PULL_UP, lid_interrupt) /* MEC1701H GPIO_0015/PWM7 OK */
GPIO_INT(AC_PRESENT,        PIN(014), GPIO_INT_BOTH, extpower_interrupt) /* MEC1701H GPIO_0014/PWM6/GPTP-IN6 OK */

/* MEC1701H GPIO_0036/RC_ID2 OK */
/* Kabylake bringup move to gpio input */
/* GPIO_INT(WP_L,              PIN(036), GPIO_INT_BOTH, switch_interrupt) */

/* Buffered power button input from PMIC / ROP_EC_PWR_BTN_L_R */
/* MEC1701H GPIO_0023/GPTP-IN1/MVP_VR_ON OK */
GPIO_INT(POWER_BUTTON_L,    PIN(023), GPIO_INT_BOTH, power_button_interrupt)

/* RSMRST from PMIC */
/* MEC1701H GPIO_0057/VCC_PWRGD OK */
GPIO_INT(RSMRST_L_PGOOD,    PIN(0126),  GPIO_INT_BOTH, power_signal_interrupt)

/* Remove. SLP_S3,S4 are eSPI virtual wires
GPIO_INT(PCH_SLP_S4_L,      PIN(0200), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_S3_L,      PIN(0206), GPIO_INT_BOTH, power_signal_interrupt)
*/

/* MEC1701H GPIO_0175/KSO17 OK */
GPIO_INT(PCH_SLP_SUS_L,     PIN(0175), GPIO_INT_BOTH, power_signal_interrupt)

GPIO_INT(ALL_SYS_PWRGD, PIN(057), GPIO_INT_BOTH, power_signal_interrupt)

/* Kabylake bring up move to ordinary GPIO input */
/* MEC1701H GPIO_0034/RC_ID1/SPI0_CLK */
/* GPIO_INT(VOLUME_UP_L,       PIN(034), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt) */
/* MEC1701H GPIO_035/PWM8/CTOUT1 OK */
/* GPIO_INT(VOLUME_DOWN_L,     PIN(035),  GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt) */

GPIO_INT(PMIC_INT_L,        PIN(0161), GPIO_INT_FALLING, power_signal_interrupt) /* MEC1701H GPIO_0161/VCI_IN2# OK */
GPIO_INT(PD_MCU_INT,        PIN(0162), GPIO_INT_FALLING | GPIO_PULL_UP, pd_mcu_interrupt) /* MEC1701H GPIO_0162/VCI_IN1# OK */
GPIO_INT(USB_C0_VBUS_WAKE_L,PIN(0242), GPIO_INT_BOTH, vbus0_evt) /* MEC1701H GPIO_0242/ISPI_CLK OK */
GPIO_INT(USB_C1_VBUS_WAKE_L,PIN(0243), GPIO_INT_BOTH, vbus1_evt) /* MEC1701H GPIO_0243/ISPI_IO0 OK */
GPIO_INT(USB_C0_BC12_INT_L, PIN(0240), GPIO_INT_FALLING, usb0_evt) /* MEC1701H GPIO_0240/INT_IO3 OK */
GPIO_INT(USB_C1_BC12_INT_L, PIN(0241), GPIO_INT_FALLING, usb1_evt) /* MEC1701H GPIO_0241/ISPI_CS# OK */

GPIO_INT(TABLET_MODE_L,     PIN(0100), GPIO_INT_BOTH | GPIO_PULL_UP, tablet_mode_interrupt) /* MEC1701H GPIO_0100/nEC_SCI OK */
/* Delayed PWR_OK from PMIC */
GPIO_INT(PMIC_DPWROK,       PIN(0151), GPIO_INT_BOTH, power_signal_interrupt) /* MEC1701H GPIO_0151/ICT4/KSO15 OK */
/* UART input */
GPIO_INT(UART0_RX,          PIN(0105), GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP, uart_deepsleep_interrupt) /* MEC1701H GPIO_0105/UART0_RX OK */


/* GPIO Pins not interrupt enabled */

/* Kabylake bring up move to ordinary GPIO input */
GPIO(VOLUME_UP_L,       PIN(034), GPIO_INPUT | GPIO_PULL_UP)
GPIO(VOLUME_DOWN_L,     PIN(035), GPIO_INPUT | GPIO_PULL_UP)
GPIO(WP_L,              PIN(036), GPIO_INPUT)

/* PCH and CPU pins */

/* Kabylake bringup using MECC board */
/*
GPIO(ALL_SYS_PWRGD, PIN(057), GPIO_INPUT)
*/

/*
 * This pulldown should be removed and SLP_S0 should be enabled as a power
 * signal interrupt in future hardware followers. The signal is pulled up in
 * the SoC when the primary rails are on and/or ramping.
 * In order to not get interrupt storms there should be external logic
 * which makes this a true binary signal into the EC.
 */
GPIO(PCH_SLP_S0_L,          PIN(0222), GPIO_INPUT | GPIO_PULL_DOWN) /* MEC1701H GPIO_0222 Func1 SER_IRQ OK */
GPIO(PCH_PWRBTN_L,          PIN(043),  GPIO_OUTPUT) /* MEC170H GPIO_0043 Func1 SB-TSI_CLK OK */
/* CONFLICT with KSI3. SCI becomes eSPI Virtual Wire, remove
GPIO(PCH_SCI_L,             PIN(026),  GPIO_ODR_HIGH)
*/
/* When asserted, ME does not lock security descriptor */
GPIO(PCH_SEC_DISABLE_L,     PIN(022),  GPIO_OUT_HIGH) /* MEC1701H GPIO_0022/GPTP-IN0 OK */
GPIO(PCH_WAKE_L,            PIN(016),  GPIO_ODR_HIGH) /* MEC1701H GPIO_0016/GPTP-IN7/SHD_IO3 OK */
GPIO(PCH_ACOK,              PIN(0110), GPIO_OUT_LOW) /* MEC1701H GPIO_0110/PS2_CLK2 OK */
GPIO(PCH_RSMRST_L,          PIN(012), GPIO_OUT_LOW) /* MEC1701H GPIO_0012/SMB07_DATA OK */
/* !!!!! Don't use GPIO_0024 if testing on MEC51xx parts because it is connected 
 * in the package to nRESET_IN# and will reset the part when driven low !!!!!
GPIO(PCH_RTCRST,            PIN(024), GPIO_OUT_LOW)
*/
GPIO(PCH_RTCRST,            PIN(052), GPIO_OUT_LOW)

/* MEC1322 Func1=nSMI, is code switching pin to Func1? Conflict with KSO04 on MEC1701H
 * eSPI replaces this signal with eSPI Virtual Wire.
GPIO(PCH_SMI_L,             PIN(044),  GPIO_ODR_HIGH)
*/
/* RCIN# line to PCH for 8042 emulation. Becomes VWire on eSPI
GPIO(PCH_RCIN_L,            PIN(0135), GPIO_ODR_HIGH)
*/
GPIO(CPU_PROCHOT,           PIN(0111), GPIO_OUT_LOW) /* MEC1701H GPIO_0111/PS2_DAT2 OK */

/* MEC1701H GPIO_0025/TIN0/nEM_INT/UART_CLK OK */
/* Kabylake with MECC board change to GPIO_OUT_LOW
GPIO(SYS_RESET_L,           PIN(025), GPIO_ODR_HIGH)
*/
GPIO(SYS_RESET_L,           PIN(025), GPIO_OUT_LOW)

/* PMIC pins */
GPIO(PMIC_SLP_SUS_L,        PIN(033), GPIO_OUT_LOW) /* GPIO_0033/RC_ID0 OK */
/*
 * BATLOW_L and ROP_LDO_EN are stuffing options. Set as input to dynamically
 * handle the stuffing option based on board id. As both signals have external
 * pulls setting this pin as input won't harm anything.
 */
GPIO(BATLOW_L_PMIC_LDO_EN,  PIN(0163),  GPIO_INPUT) /* MEC1701H GPIO_0163/VCI_IN0# OK */


/* I2C pins - these will be reconfigured for alternate function below */
GPIO(SMB00_SCL,             PIN(004),  GPIO_INPUT)  /* MEC1701H */
GPIO(SMB00_SDA,             PIN(003),  GPIO_INPUT)  /* MEC1701H */
GPIO(SMB02_SCL,             PIN(0155), GPIO_INPUT)  /* MEC1701H */
GPIO(SMB02_SDA,             PIN(0154), GPIO_INPUT)  /* MEC1701H */
GPIO(SMB03_SCL,             PIN(010),  GPIO_INPUT)  /* MEC1701H */
GPIO(SMB03_SDA,             PIN(007),  GPIO_INPUT)  /* MEC1701H */
GPIO(SMB04_SCL,             PIN(0144), GPIO_INPUT)  /* MEC1701H */
GPIO(SMB04_SDA,             PIN(0143), GPIO_INPUT)  /* MEC1701H */
GPIO(SMB05_SCL,             PIN(0142), GPIO_INPUT)  /* MEC1701H */
GPIO(SMB05_SDA,             PIN(0141), GPIO_INPUT)  /* MEC1701H */


/* USB and USB-PD related pins */
GPIO(PD_RST_L,              PIN(0153), GPIO_ODR_HIGH) /* MEC1701H GPIO_0153/LED2 OK */
GPIO(USB2_OTG_ID,           PIN(013),  GPIO_ODR_LOW)  /* MEC1701H GPIO_0013/SMB07_CLK/TOUT2 OK */
GPIO(USB1_ENABLE,           PIN(042),  GPIO_OUT_LOW) /* MEC1701H GPIO_0042/PECI_DAT/SB-TSI_DAT */
GPIO(USB2_ENABLE,           PIN(067),  GPIO_OUT_LOW) /* MEC1701H GPIO_0067/CLKRUN# OK */
GPIO(USB_C0_DP_HPD,         PIN(0101), GPIO_OUT_LOW) /* MEC1701H GPIO_0101/BGP01 OK */
GPIO(USB_C1_DP_HPD,         PIN(0102), GPIO_OUT_LOW) /* MEC1701H GPIO_0102/BGP02 OK */
GPIO(USB_PD_WAKE,           PIN(0202), GPIO_OUT_HIGH) /* MEC1701H GPIO_0202/ADC02 OK */
GPIO(USB2_OTG_VBUSSENSE,    PIN(0140), GPIO_OUT_LOW) /* MEC1701H GPIO_0140/SMB06_CLK OK */
GPIO(USB_C0_5V_EN,          PIN(0205), GPIO_OUT_LOW) /* MEC1701H GPIO_0205/ADC05 OK */
GPIO(USB_C1_5V_EN,          PIN(0206), GPIO_OUT_LOW) /* MEC1701H GPIO_0205/ADC06 OK */
GPIO(USB_C0_CHARGE_EN_L,    PIN(0165),  GPIO_OUT_LOW) /* MEC1701H GPIO_0165/32KHZ_IN/CTOUT0/TRACECLK OK */
GPIO(USB_C1_CHARGE_EN_L,    PIN(0152),  GPIO_OUT_LOW) /* MEC1701H GPIO_0152/GPTP-OUT/KSO16 OK */

/* Fan PWM output and TACH input. PROCHOT input */
GPIO(EC_FAN1_TTACH,         PIN(051), GPIO_INPUT | GPIO_PULL_UP) /* MEC1701H GPIO_0051/FAN_TACH1 OK */
/* Fan PWM output - NC / testing only */
GPIO(EC_FAN1_PWM,           PIN(054), GPIO_OUT_LOW) /* MEC1701H GPIO_0054/PWM1 OK */
/* prochot input from devices */
GPIO(PLATFORM_EC_PROCHOT,   PIN(050), GPIO_INPUT | GPIO_PULL_UP) /* MEC1701H GPIO_0050/FAN_TACH0 OK */


/* Miscellaneous */
/* KB BL PWM, only connected to TP */
GPIO(PWM_KBLIGHT,           PIN(02),   GPIO_OUT_LOW) /* MEC1701H GPIO_0002/PWM5 OK */
GPIO(ENTERING_RW,           PIN(041),  GPIO_OUT_LOW) /* MEC1701H GPIO_0041/SYS_SHDN# OK */
GPIO(ENABLE_TOUCHPAD,       PIN(0115), GPIO_OUT_LOW) /* MEC1701H GPIO_0115/PS2_DATA0A OK */
GPIO(BAT_PRESENT_L,         PIN(0200), GPIO_INPUT)   /* MEC1701H GPIO_0200/ADC00 OK */
GPIO(WLAN_OFF_L,            PIN(0164), GPIO_OUT_LOW) /* MEC1701H GPIO_0164/VCI_OVRD_IN OK */
/* From lid sensor */
GPIO(ENABLE_BACKLIGHT,      PIN(00), GPIO_OUT_LOW) /* MEC1701H GPIO_0000/VCI_IN3# OK */
/* Interrupts from accelerometer / gyro -- not yet implemented */
/* GPIO(ACCEL1_INT,            PIN(0161), GPIO_INPUT) TODO MEC1701H */
/* GPIO(ACCEL2_INT,            PIN(0127), GPIO_INPUT) TODO MEC1701H */
/* GPIO(ACCEL3_INT,            PIN(0147), GPIO_INPUT) MEC1701H must move. GPIO_0147 is JTAG_CLK */
GPIO(PP1800_DX_SENSOR_EN,   PIN(011),  GPIO_OUT_LOW) /* MEC1701H GPIO_0011/nSMI OK */
GPIO(PP3300_WLAN_EN,        PIN(0207), GPIO_OUT_LOW) /* MEC1701H GPIO_0207/ADC07 OK */
/* MEC1701H GPIO_0126/PVT_IO3/KSO13 OK
GPIO(ACCEL4_INT,            PIN(0126), GPIO_INPUT)
*/
GPIO(PP1800_DX_AUDIO_EN,    PIN(0114), GPIO_OUT_LOW) /* MEC1701H GPIO_0114/PS2_CLK0A/nEC_SCI OK */


/* Board Version */
GPIO(BOARD_VERSION1,        PIN(0130),  GPIO_INPUT) /* MEC1701H GPIO_0130/SMB10_DATA */
GPIO(BOARD_VERSION2,        PIN(0131),  GPIO_INPUT) /* MEC1701H GPIO_0131/SMB10_CLK */
GPIO(BOARD_VERSION3,        PIN(0132),  GPIO_INPUT) /* MEC1701H GPIO_0132/SMB06_DATA */

/* SPI
 * Chip select must be open drain and driven high before SPI controller
 * configuration.
 */
/* 
 * MEC1701H GPIO_0055/PWM2/SHD_CS0#/RSMRST#
 * MEC1701H QMSPI controller drives chip select, must be 
 * configured to alternative function. See below.
 * GPIO_SHD_CS0 is used in board level spi_devices[] table
 */
GPIO(SHD_CS0,		PIN(055),  GPIO_ODR_HIGH)


/*
 * TODO(crosbug.com/p/40848): These LEDs should be under control of the mec1322
 * LED control unit. Remove these GPIO definitions once the LED control unit
 * is functional.
 */
GPIO(CHARGE_LED_1,	PIN(0156), GPIO_OUT_LOW) /* MEC1701H GPIO_0156/LED0 OK */
GPIO(CHARGE_LED_2,	PIN(0157), GPIO_OUT_LOW) /* MEC1701H GPIO_0157/LED1 OK */



/* Alternate functions GPIO definitions */

/* KB pins */
#define GPIO_KB_INPUT (GPIO_INPUT | GPIO_PULL_UP)
#define GPIO_KB_OUTPUT (GPIO_ODR_HIGH)

/*
 * TODO
 * From chip/mec1701/config_chip.h
 * #define GPIO_PIN_MASK(pin, mask) (pin), (mask)
 * pin is the MEC17xx GPIO bank where each bank has 32 pins
 * mask is the mask of pin in the bank.
 * Pin number from data sheet is octal, convert to hex.
 * bank = pin number in hex >> 5
 * pin bit position in 32-bit bank = octal pin number in hex & 0x1F
 */

/* !!! Ported to MEC1701H UART0 !!! */
/*
 * GPIO_0104(UART0_TX) Func1
 * GPIO_0105(UART0_RX) Func1
 * Bank 2 bits[4:5]
*/
ALTERNATE(PIN_MASK(2, 0x30), 1, MODULE_UART, 0)

/* KB pins */
/* !!! Ported to MEC1701H !!!
 * MEC1322 GPIO_0000 - 0005 Func3 are KSO00, KSO06, KSO07, KSO08, KSO10, KSO12
 * MEC1322 GPIO_0100 - 0104 Func3 are KSO01, KSO02, KSO03, KSO04, KSO05
 * MEC1322 GPIO_0106 - 0107 Func3 are KSO9, KSO11
 * MEC1322 GPIO_0032 Func3 is KSI3
 * MEC1322 GPIO_0040 Func3 is KSI5
 * MEC1322 GPIO_0042 - 0043 Func3 are KSI6, KSI7
 * MEC1322 GPIO_0125 - 0126 Func2 are KSI0, KSI1
 * MEC1322 GPIO_0142 Func3 is KSI4
 * MEC1322 GPIO_0144 Func3 is KSI2
 *
 * MEC1704H KSO00 = GPIO_0040 Func2
 *          KSO01 = GPIO_0045 Func1
 *          KSO02 = GPIO_0046 Func2
 *          KSO03 = GPIO_0047 Func2
 *          KSO04 = GPIO_0107 Func2 !!! Func1 = nSMI !!!
 *          KSO05 = GPIO_0112 Func2
 *          KSO06 = GPIO_0113 Func2
 *          KSO07 = GPIO_0120 Func1
 *          KSO08 = GPIO_0121 Func2 !!! Func1 = PVT_IO0 (MOSI) !!!
 *          KSO09 = GPIO_0122 Func3 !!! Func2 = PVT_IO1 (MISO) !!!
 *          KSO10 = GPIO_0123 Func3 !!! Func2 = PVT_IO2 (SPI IO2/WP#) use? !!!
 *          KSO11 = GPIO_0124 Func3 !!! Func2 = PVT_CS# !!!
 *          KSO12 = GPIO_0125 Func3 !!! Func2 = PVT_CLK !!!
 * We are moving SPI to QMSPI Port 0 (Shared)
 *          KSI0 = GPIO_0017 Func2
 *          KSI1 = GPIO_0020 Func1
 *          KSI2 = GPIO_0021 Func2 Func1 = LPCPD# should not be an issue when Host interface is eSPI
 *          KSI3 = GPIO_0026 Func2
 *          KSI4 = GPIO_0027 Func2
 *          KSI5 = GPIO_0030 Func2
 *          KSI6 = GPIO_0031 Func2
 *          KSI7 = GPIO_0032 Func2
 */
/* KB ROW - KSO00, KSO02-03 - GPIO_0040, 0046-0047 Func2, Bank 1 bits[0,6:7] */
ALTERNATE(PIN_MASK(1, 0xc1), 2, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* KB ROW - KSO01 - GPIO_0045 Func1, Bank 1 bit[5] */
ALTERNATE(PIN_MASK(1, 0x20), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* KB ROW - KSO04-06,08 - GPIO_0107,0112,0113,0121 Func2, Bank 2 bits[7,10:11,17] */
ALTERNATE(PIN_MASK(2, 0x20c80), 2, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* KB ROW - KSO07 - GPIO_0120 Func1, Bank 2 bit[16] */
ALTERNATE(PIN_MASK(2, 0x10000), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* KB ROW - KSO09-12 - GPIO_0122-0125 Func3, Bank 2 bits[18:21] */
ALTERNATE(PIN_MASK(2, 0x3c0000), 3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* KB COL - KSI1 - GPIO_0020 Func1, Bank 0 bit[16] */
ALTERNATE(PIN_MASK(0, 0x10000), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
/* KB COL - KSI0,KSI2-KSI7 - GPIO_0017,0021,0026,0027,0030,0031,0032
 * Func2, Bank 0 bits[15,17,22,23,24:26] */
ALTERNATE(PIN_MASK(0, 0x7c28000), 2, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)


/* !!! PORTED TO MEC1701H !!! */
/* eSPI pins */
/* ESPI_RESET# - GPIO_0061 Function 2, Bank 1 bit[17] */
ALTERNATE(PIN_MASK(1, 0x20000), 2, MODULE_LPC, 0)
/* ESPI_ALERT# - GPIO_0063 Function 2, Bank 1 bit[19] */
ALTERNATE(PIN_MASK(1, 0x80000), 2, MODULE_LPC, 0)
/* ESPI_CS# - GPIO_0066 Function 2, Bank 1 bit[22] */
ALTERNATE(PIN_MASK(1, 0x400000), 2, MODULE_LPC, 0)
/* ESPI_CLK - GPIO_0065 Function 2, Bank 1 bit[21] */
ALTERNATE(PIN_MASK(1, 0x200000), 2, MODULE_LPC, 0)
/* ESPI_IO{0,1,2,3} - GPIO_0070-GPIO_0073 Function 2, Bank 1 bits[24:27] */
ALTERNATE(PIN_MASK(1, 0xf000000), 2, MODULE_LPC, 0)

/* LPC LRESET# GPIO_0064/LRESET#
 * Function 1, Bank 1 bit[20] */
ALTERNATE(PIN_MASK(1, 0x100000), 1, MODULE_LPC, GPIO_PULL_UP)



/* !!! PORTED TO MEC1701H !!! */
/* MECC card SPI flash is connected to QMSPI Shared Port
 * Also, MEC1701H Private SPI Port (Port 1) has pins multiplexed
 * with KSO pins used in key scan!
 * QMSPI Shared SPI Port (Port 0)
 * NOTE: QMSPI Shared SPI Port pins are on VTR2
 */
/*
 * MEC1701H SHD SPI is connected to QMSPI controller.
 * QMSPI drives chip select. SHD_CS0#(GPIO_0055) must be set 
 * to alternate function 2 and GPIO_ODR_HIGH.
 * GPIO_0055 Function 2, Bank 1 bit[13]
 */
ALTERNATE(PIN_MASK(1, 0x2000), 2, MODULE_SPI, GPIO_ODR_HIGH)
/* MOSI(SHD_IO0) - GPIO_0223 Function 2, Bank 4 bit[19] */
ALTERNATE(PIN_MASK(4, 0x80000), 2, MODULE_SPI, 0)
/* MISO(SHD_IO1) - GPIO_0224 Function 2, Bank 4 bit[20] */
ALTERNATE(PIN_MASK(4, 0x100000), 2, MODULE_SPI, 0)
/* SHD_CLK - GPIO_0056 Function 2, Bank 1 bit[14] */
ALTERNATE(PIN_MASK(1, 0x4000), 2, MODULE_SPI, 0)
/* SHD_CS0# - GPIO_0055 Function 2, Shared SPI chip select, Bank 1 bit[13] */
ALTERNATE(PIN_MASK(1, 0x2000), 2, MODULE_SPI, 0)

/* !!! PORTED TO MEC1701H !!! */
/* I2C pins */
/*
 * SMB00_DAT - GPIO_0003 Func1, SMB00_CLK - GPIO_0004 Func1
 * SMB03_DAT - GPIO_0007 Func1, SMB03_CLK - GPIO_0010 Func1
 * Bank 0 bits[3:4,7:8]
*/
ALTERNATE(PIN_MASK(0,  0x0198), 1, MODULE_I2C, GPIO_ODR_HIGH)
/*
 * SMB05_DAT - GPIO_0141 Func1, SMB05_CLK - GPIO_0142 Func1
 * SMB04_DAT - GPIO_0143 Func1, SMB04_CLK - GPIO_0144 Func1
 * SMB02_DAT - GPIO_0154 Func1, SMB02_CLK - GPIO_0155 Func1
 * Bank 3 bits[1:4,12:13]
*/
ALTERNATE(PIN_MASK(3,  0x301e), 1, MODULE_I2C, GPIO_ODR_HIGH)

/* !!! PORTED TO MEC1701H !!! */
/* ADC pins */
/* ADC01 - GPIO_0201 / PPVAR_BOOSTIN_SENSE. Func1 Bank 4 bit[1]
 * ADC03 - GPIO_0203 / IADP_ACMON_BMON. Func1 Bank 4 bit[3]
 * ADC04 - GPIO_0204 / PMON_PSYS. Func1 Bank 4 bit[4]
 */
ALTERNATE(PIN_MASK(4,  0x001a), 1, MODULE_ADC, GPIO_ANALOG)

/* !!! PORTED TO MEC1701H !!! */
/* LED0 - GPIO_0156 Func1 Bank 3 bit[14]
 * LED1 - GPIO_0157 Func1 Bank 3 bit[15]
 */
ALTERNATE(PIN_MASK(3, 0xc000), 1, MODULE_POWER_LED, 0)

/* nRESET_OUT functionality is PWROK signal from MEC1701H */
/* nRESET_OUT - GPIO121 Bank 2 bit[17] */
/* MEC1701H nRESET_OUT is GPIO_0106/PWROK func1
 * Bank 2 bit[6] */
/* Kabylake MEC1701H eSPI board disable nRESET_OUT
ALTERNATE(PIN_MASK(2, 0x40), 1, MODULE_PMU, 0)
*/
