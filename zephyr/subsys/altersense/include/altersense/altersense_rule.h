/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SUBSYS_ALTERSENSE_INCLUDE_ALTERSENSE_RULE_H_
#define ZEPHYR_SUBSYS_ALTERSENSE_INCLUDE_ALTERSENSE_RULE_H_

#include <device.h>
#include <errno.h>

typedef bool (*alternsense_rule_predicate)(const struct device *dev,
					   const struct device *secondary);

struct alternsense_rule_api {
	alternsense_rule_predicate predicate;
};

/**
 * @brief Test the current rule
 *
 * @param dev
 * @return True if the rule passed
 */
__syscall bool altersense_rule_test(const struct device *dev,
				    const struct device *secondary);

static inline bool z_impl_alternsense_rule_test(const struct device *dev,
						const struct device *secondary)
{
	const struct alternsense_rule_api *api =
		(const struct alternsense_rule_api *)dev->api;

	if (!api->predicate) {
		return -ENOTSUP;
	}
	return api->predicate(dev, secondary);
}

#include <syscalls/altersense_rule.h>
#endif // ZEPHYR_SUBSYS_ALTERSENSE_INCLUDE_ALTERSENSE_RULE_H_
