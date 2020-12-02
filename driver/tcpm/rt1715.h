/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * RT1715 TCPC Driver
 */

#ifndef __CROS_EC_USB_PD_TCPM_RT1715_H
#define __CROS_EC_USB_PD_TCPM_RT1715_H

#define RT1715_VENDOR_ID 0x29CF

/* RT1715 Private RegMap */

#define RT1715_REG_PHY_CTRL1			0x80

#define RT1715_REG_BMC_CTRL			0x90
#define RT1715_REG_BMCIO_RXDZSEL		0x93
#define RT1715_REG_VCONN_CLIMITEN       0x95

#define RT1715_REG_IDLE_CTRL			0x9B
#define RT1715_REG_I2CRST_CTRL			0X9E

#define RT1715_REG_SWRESET			0xA0
#define RT1715_REG_TTCPC_FILTER			0xA1
#define RT1715_REG_DRP_TOGGLE_CYCLE		0xA2
#define RT1715_REG_DRP_DUTY_CTRL		0xA3
#define RT1715_REG_BMCIO_RXDZEN			0xAF


/*
 * RT1715_REG_PHY_CTRL1				0x80
 */

#define RT1715_REG_PHY_CTRL1_SET(retry_discard, toggle_cnt, bus_idle_cnt,      \
				 rx_filter)                                    \
	((retry_discard << 7) | (toggle_cnt << 4) | (bus_idle_cnt << 2) |      \
	 (rx_filter & 0x03))

#define RT1715_I2C_ADDR_FLAGS			0x4E
	 
/*
 * RT1715_REG_BMC_CTRL				0x90
 */

//#define RT1715_REG_IDLE_EN			BIT(6)
#define RT1715_REG_DISCHARGE_EN			BIT(5)
#define RT1715_REG_BMCIO_LPRPRD			BIT(4)
#define RT1715_REG_BMCIO_LPEN			BIT(3)
#define RT1715_REG_BMCIO_BG_EN			BIT(2)
#define RT1715_REG_VBUS_DET_EN			BIT(1)
#define RT1715_REG_BMCIO_OSC_EN			BIT(0)
#define RT1715_REG_BMC_CTRL_DEFAULT                                            \
	(RT1715_REG_BMCIO_BG_EN | RT1715_REG_VBUS_DET_EN |                     \
	 RT1715_REG_BMCIO_OSC_EN)

/*
 * RT1715_REG_BMCIO_RXDZSEL			0x93
 */

#define RT1715_MASK_OCCTRL_SEL			0xE0
#define RT1715_OCCTRL_600MA			0x80
#define RT1715_MASK_BMCIO_RXDZSEL		BIT(0)

/*
 * RT1715_REG_IDLE_CTRL				0x9B
 */

#define RT1715_REG_CK_300K_SEL			BIT(7)
#define RT1715_REG_SHIPPING_OFF			BIT(5)
#define RT1715_REG_ENEXTMSG			BIT(4)
#define RT1715_REG_AUTOIDLE_EN			BIT(3)

/* timeout = (tout*2+1) * 6.4ms */
#ifdef CONFIG_USB_PD_REV30
#define RT1715_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout)                  \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07) |   \
	 RT1715_REG_ENEXTMSG)
#else
#define RT1715_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout)                  \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))
#endif

/*
 * RT1715_REG_I2CRST_CTRL			0x9E
 */

#define RT1715_REG_I2CRST_EN			BIT(7)

/* timeout = (tout+1) * 12.5ms */
#define RT1715_REG_I2CRST_SET(en, tout)		((en << 7) | (tout & 0x0f))

extern const struct tcpm_drv rt1715_tcpm_drv;

#endif /* __CROS_EC_USB_PD_TCPM_RT1715_H */
