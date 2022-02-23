/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_POWER_SIGNALS_H__
#define __AP_PWRSEQ_POWER_SIGNALS_H__

#include <devicetree.h>
#include <power_signals_ext.h>

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq)

/**
 * @brief Definitions for AP power sequence signals.
 *
 * Defines the enums for the AP power sequence signals.
 * Generates enums from the device tree 'intel,ap-pwrseq'
 * compatible child nodes.
 */

/**
 * @brief Generate the enum for this power signal.
 */
#define PWR_SIGNAL_ENUM(id) \
	 DT_STRING_UPPER_TOKEN(id, enum_name)

#define PWR_SIGNAL_ENUM_COMMA(id) \
	 PWR_SIGNAL_ENUM(id),
/**
 * @brief Enum of all power signals
 *
 * Defines the enums of all the power signals configured
 * in the system. Uses the 'enum-name' property to name
 * the signal.
 */
enum power_signal {
DT_FOREACH_CHILD(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	PWR_SIGNAL_ENUM_COMMA)
	POWER_SIGNAL_COUNT,
};

#undef PWR_SIGNAL_ENUM_COMMA

/*
 * Prepended tags to the signal enum for
 * each source of signal.
 */
#define TAG_GPIO_PWR_SIGNAL	PWR_GPIO_
#define TAG_VW_PWR_SIGNAL	PWR_VW_

/**
 * @brief Generate the enum for this source power signal.
 */
#define ADD_TAG_TO_PWR_ENUM(tag, name) DT_CAT(tag, name)

#define SRC_PWR_SIGNAL_ENUM(id, tag) \
	 ADD_TAG_TO_PWR_ENUM(tag, PWR_SIGNAL_ENUM(id))

#define GEN_TAG_PWR_ENUM(id, compat, tag)	\
	COND_CODE_1(DT_NODE_HAS_COMPAT(id, compat), \
	(SRC_PWR_SIGNAL_ENUM(id, tag), ),	\
	())

/**
 * @brief Enum of power signal GPIOs
 *
 * Defines the enums of all the GPIOs configured
 * as power signals. Input GPIOs are defined first,
 * followed by output GPIOs. This is so input GPIOs can
 * be used as interrupts with their own callback array.
 */
enum power_signal_gpios {
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	GEN_TAG_PWR_ENUM, intel_ap_pwrseq_input_gpio, TAG_GPIO_PWR_SIGNAL)
/*
 * End of the input GPIOs.
 * An enum is generaed that represents the number of input GPIOs.
 */
	GPIO_INPUT_POWER_SIGNAL_COUNT,
/*
 * Restart the enum count so that there is no gap.
 */
	_GPIO_POWER_SIGNAL_OUTPUTS = GPIO_INPUT_POWER_SIGNAL_COUNT - 1,

DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	GEN_TAG_PWR_ENUM, intel_ap_pwrseq_output_gpio, TAG_GPIO_PWR_SIGNAL)
};

/**
 * @brief Enum of power signal virtual wire signals
 *
 * Defines the enums of all the virtual wire signals.
 */
enum power_signal_vw {
DT_FOREACH_CHILD_VARGS(DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq),
	GEN_TAG_PWR_ENUM, intel_ap_pwrseq_vw, TAG_VW_PWR_SIGNAL)
	VW_POWER_SIGNAL_COUNT
};

#undef GEN_TAG_PWR_ENUM

#endif /* DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq) */

#endif /* __AP_PWRSEQ_POWER_SIGNALS_H__ */
