/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "power.h"
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>

#if DT_NODE_EXISTS(S5_DOMAIN)

static int s5_domain_init(const struct device *dev)
{
	/* The chip starts in G3 state so init the S5 domain as suspended */
	pm_device_init_suspended(dev);
	pm_device_runtime_enable(dev);

	return 0;
}

static int s5_domain_pm_action(const struct device *dev,
			       enum pm_device_action action)
{
	/* TODO: Add pm_device_children_action_run calls for the PM actions,
	 * once drivers of devices on the domain are upstream and defines PM
	 * structures/handles PM actions
	 */
	ARG_UNUSED(dev);
	ARG_UNUSED(action);

	return 0;
}

PM_DEVICE_DT_DEFINE(S5_DOMAIN, s5_domain_pm_action);
DEVICE_DT_DEFINE(S5_DOMAIN, s5_domain_init, PM_DEVICE_DT_GET(S5_DOMAIN), NULL, NULL,
		 POST_KERNEL, 10, NULL);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(TEMP_SENSORS_COMPAT) */
