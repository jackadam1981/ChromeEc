/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>

const struct gpio_dt_spec *
sm5803_emul_get_interrupt_gpio(const struct emul *emul);
struct i2c_common_emul_data *sm5803_emul_get_i2c_main(const struct emul *emul);
struct i2c_common_emul_data *sm5803_emul_get_i2c_chg(const struct emul *emul);
struct i2c_common_emul_data *sm5803_emul_get_i2c_meas(const struct emul *emul);

/**
 * Read the value of a charger page register, by address.
 *
 * This is useful to verify that a user has written an expected value to a
 * register, without depending on the user's corresponding getter function.
 *
 * @return negative value on error, otherwise 8-bit register value.
 */
int sm5803_emul_read_chg_reg(const struct emul *emul, uint8_t reg);

/**
 * Set the reported VBUS voltage, in mV.
 *
 * If the VBUS voltage crosses the charger detection threshold as a result,
 * a CHG_DET interrupt will automatically be triggered.
 */
void sm5803_emul_set_vbus_voltage(const struct emul *emul, uint16_t mv);

/** Set the GPADC enable bits in GPADC_CONFIG_1 and GPADC_CONFIG_2 registers. */
void sm5803_emul_set_gpadc_conf(const struct emul *emul, uint8_t conf1,
				uint8_t conf2);

/**
 * Set the INT_REQ_* registers to indicate pending interrupts.
 *
 * This does not clear pending IRQs; it only asserts them. IRQs are cleared only
 * when the interrupt status registers are read.
 */
void sm5803_emul_set_irqs(const struct emul *emul, uint8_t irq1, uint8_t irq2,
			  uint8_t irq3, uint8_t irq4);
