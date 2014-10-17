/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for IT83xx. */

#ifndef __CROS_EC_IT83XX_PWM_H
#define __CROS_EC_IT83XX_PWM_H

/* Data structure to define PWM channel control registers. */
struct pwm_ctrl_t {
	/* PWM channel output duty register. */
	volatile unsigned char *pwm_duty;
	/* PWM channel clock source selection register. */
	volatile unsigned char *pwm_clock_source;
	/* PWM channel pin control register. */
	volatile unsigned char *pwm_pin;
};

/* Data structure to define PWM channel control registers part 2. */
struct pwm_ctrl_t2 {
	/* PWM cycle time register. */
	volatile unsigned char *pwm_cycle_time;
	/* PWM channel clock prescaler register (LSB). */
	volatile unsigned char *pwm_cpr_lsb;
	/* PWM channel clock prescaler register (MSB). */
	volatile unsigned char *pwm_cpr_msb;
	/* PWM prescaler clock frequency select register. */
	volatile unsigned char *pwm_pcfsr_reg;
	/* PWM prescaler clock frequency select register setting. */
	volatile unsigned char pwm_pcfsr_ctrl;
};

/* Data structure to define PWM channels. */
struct pwm_t {
	/* PWM channel ID */
	int channel;
	/* PWM channel flags. See include/pwm.h */
	uint32_t flags;
};

extern const struct pwm_t pwm_channels[];

#endif /* __CROS_EC_IT83XX_PWM_H */
