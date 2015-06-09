/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB9281 USB port switch.
 */

#ifndef PI3USB9281_H
#define PI3USB9281_H

#define PI3USB9281_REG_DEV_ID       0x01
#define PI3USB9281_REG_CONTROL      0x02
#define PI3USB9281_REG_INT          0x03
#define PI3USB9281_REG_INT_MASK     0x05
#define PI3USB9281_REG_DEV_TYPE     0x0a
#define PI3USB9281_REG_CHG_STATUS   0x0e
#define PI3USB9281_REG_MANUAL       0x13
#define PI3USB9281_REG_RESET        0x1b
#define PI3USB9281_REG_VBUS         0x1d

#define PI3USB9281_DEV_ID           0x10
#define PI3USB9281_DEV_ID_A         0x18

#define PI3USB9281_CTRL_INT_DIS     (1 << 0)
#define PI3USB9281_CTRL_AUTO        (1 << 2)
#define PI3USB9281_CTRL_SWITCH_AUTO (1 << 4)
/* Bits 5 thru 7 are read X, write 0 */
#define PI3USB9281_CTRL_MASK        0x1f
/* Bits 1 and 3 are read 1, write 1 */
#define PI3USB9281_CTRL_RSVD_1      0x0a

#define PI3USB9281_PIN_MANUAL_VBUS  (3 << 0)
#define PI3USB9281_PIN_MANUAL_DP    (1 << 2)
#define PI3USB9281_PIN_MANUAL_DM    (1 << 5)

#define PI3USB9281_INT_ATTACH       (1 << 0)
#define PI3USB9281_INT_DETACH       (1 << 1)
#define PI3USB9281_INT_OVP          (1 << 5)
#define PI3USB9281_INT_OCP          (1 << 6)
#define PI3USB9281_INT_OVP_OC       (1 << 7)

#define PI3USB9281_TYPE_NONE        0
#define PI3USB9281_TYPE_MHL         (1 << 0)
#define PI3USB9281_TYPE_OTG         (1 << 1)
#define PI3USB9281_TYPE_SDP         (1 << 2)
#define PI3USB9281_TYPE_CAR         (1 << 4)
#define PI3USB9281_TYPE_CDP         (1 << 5)
#define PI3USB9281_TYPE_DCP         (1 << 6)

#define PI3USB9281_CHG_NONE         0
#define PI3USB9281_CHG_CAR_TYPE1    (1 << 1)
#define PI3USB9281_CHG_CAR_TYPE2    (3 << 0)
#define PI3USB9281_CHG_APPLE_1A     (1 << 2)
#define PI3USB9281_CHG_APPLE_2A     (1 << 3)
#define PI3USB9281_CHG_APPLE_2_4A   (1 << 4)
/* Check if charge status has any connection */
#define PI3USB9281_CHG_STATUS_ANY(x) (((x) & 0x1f) > 1)

/* Define configuration of one pi3usb9281 part */
struct pi3usb9281_config {
	/* i2c port that chip resides on */
	int i2c_port;
	/* GPIO for chip selection in muxed configuration */
	enum gpio_signal mux_gpio;
	/* Logic level of mux_gpio to select chip */
	int mux_gpio_level;
	/* Mutex to lock access to mux gpio */
	struct mutex *mux_lock;
};

/* Map port number to port configuration struct. */
struct pi3usb9281_config *board_port_to_pi3usb9281_config(int port);

/* Initialize chip. */
void pi3usb9281_init(struct pi3usb9281_config *chip);

/* Read PI3USB9281 register. */
uint8_t pi3usb9281_read(struct pi3usb9281_config *chip, uint8_t reg);

/* Write PI3USB9281 register. */
int pi3usb9281_write(struct pi3usb9281_config *chip, uint8_t reg, uint8_t val);

/* Enable interrupts. */
int pi3usb9281_enable_interrupts(struct pi3usb9281_config *chip);

/* Disable all interrupts. */
int pi3usb9281_disable_interrupts(struct pi3usb9281_config *chip);

/* Set interrupt mask. */
int pi3usb9281_set_interrupt_mask(struct pi3usb9281_config *chip, uint8_t mask);

/* Get and clear current interrupt status. */
int pi3usb9281_get_interrupts(struct pi3usb9281_config *chip);

/* Get attached device type. */
int pi3usb9281_get_device_type(struct pi3usb9281_config *chip);

/* Get attached charger status. */
int pi3usb9281_get_charger_status(struct pi3usb9281_config *chip);

/* Get charger current limit based on device type and charger status. */
int pi3usb9281_get_ilim(int device_type, int charger_status);

/* Set switch configuration to manual. */
int pi3usb9281_set_switch_manual(struct pi3usb9281_config *chip, int val);

/* Set bits to enable pins in manual switch register. */
int pi3usb9281_set_pins(struct pi3usb9281_config *chip, uint8_t mask);

/* Set D+/D-/Vbus switches to open or closed/auto-control. */
int pi3usb9281_set_switches(struct pi3usb9281_config *chip, int open);

/* Reset PI3USB9281. */
int pi3usb9281_reset(struct pi3usb9281_config *chip);

#endif /* PI3USB9281_H */
