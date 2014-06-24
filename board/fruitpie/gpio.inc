/* -*- mode:c -*- */

GPIO(VBUS_WAKE,         B, 5,  INT_BOTH, vbus_event)
GPIO(MASTER_I2C_INT_L,  C, 13, INT_FALLING, tsu_event)

/* PD RX/TX */
GPIO(USB_CC1_PD,        A, 0,  ANALOG,  NULL)
GPIO(PD_REF1,           A, 1,  ANALOG,  NULL)
GPIO(PD_REF2,           A, 3,  ANALOG,  NULL)
GPIO(USB_CC2_PD,        A, 4,  ANALOG,  NULL)
GPIO(PD_CLK_OUT,        B, 9,  OUT_LOW, NULL)
GPIO(PD_TX_EN,          B, 12, OUT_LOW, NULL)
GPIO(PD_TX_DATA,        B, 14, OUT_LOW, NULL)
#if 0
GPIO(PD_CLK_IN,         B, 13, OUT_LOW, NULL)
#endif

/* Power and muxes control */
GPIO(PP5000_EN,         A, 5,  OUT_HIGH, NULL)
GPIO(CC_HOST,           A, 6,  OUT_LOW,  NULL)
GPIO(CHARGE_EN_L,       A, 8,  OUT_LOW,  NULL)
GPIO(USB_C_5V_EN,       A, 10, OUT_LOW,  NULL)
GPIO(VCONN1_EN,         B, 15, OUT_LOW,  NULL)
GPIO(VCONN2_EN,         C, 14, OUT_LOW,  NULL)
GPIO(SS1_EN_L,          A, 9,  OUT_HIGH, NULL)
GPIO(SS2_EN_L,          B, 4,  OUT_HIGH, NULL)
GPIO(SS2_USB_MODE_L,    B, 3,  OUT_HIGH, NULL)
GPIO(SS1_USB_MODE_L,    B, 8,  OUT_HIGH, NULL)
GPIO(DP_MODE,           C, 15, OUT_LOW,  NULL)
GPIO(DP_POLARITY_L,     A, 7,  OUT_HIGH, NULL)

/* Not used : no host on that bus */
GPIO(SLAVE_I2C_INT_L,   B, 2,  ODR_HIGH, NULL)

/* Alternate functions */
#if 0
GPIO(USB_DM,            A, 11, ANALOG,  NULL)
GPIO(USB_DP,            A, 12, ANALOG,  NULL)
GPIO(UART_TX,           A, 14, OUT_LOW, NULL)
GPIO(UART_RX,           A, 15, OUT_LOW, NULL)
#endif
/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(SLAVE_I2C_SCL,     B, 6,  INPUT, NULL)
GPIO(SLAVE_I2C_SDA,     B, 7,  INPUT, NULL)
GPIO(MASTER_I2C_SCL,    B, 10, INPUT, NULL)
GPIO(MASTER_I2C_SDA,    B, 11, INPUT, NULL)

/* Rohm BD92104 connections */
GPIO(ALERT_L,           A, 2,  INT_FALLING, rohm_event)
GPIO(USBPD_RST,         B, 0,  OUT_LOW,     NULL)
GPIO(USBPD_FORCE_OTG,   B, 1,  OUT_LOW,     NULL)
GPIO(USBPD_VIN_EN_L,    F, 0,  OUT_HIGH,    NULL)

/* Test points */
GPIO(TP9,               A, 13, ODR_HIGH, NULL)
GPIO(TP11,              F, 1,  ODR_HIGH, NULL)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

#undef GPIO
#undef UNIMPLEMENTED
