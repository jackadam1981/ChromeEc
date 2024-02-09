/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI host command */

#include "hooks.h"
#include "include/pd_driver.h"
#include "include/platform.h"
#include "include/ppm.h"
#include "ppm_common.h"
#include "usb_pd.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ucsi, LOG_LEVEL_INF);

static struct ucsi_ppm_driver *eppm_drv;

struct ucsi_pd_driver *rts54xx_open(void);

/* Sort of main */
void eppm_init(void)
{
	struct ucsi_pd_driver *drv;

	drv = rts54xx_open();
	if (!drv) {
		LOG_ERR("Failed to open rts5453");
		return;
	}

	/* Start a PPM task. */
	if (drv->init_ppm(drv->dev)) {
		LOG_ERR("Failed to init PPM");
		return;
	}

	LOG_INF("Initialized PPM");

	eppm_drv = drv->get_ppm(drv->dev);
	eppm_drv->register_notify(eppm_drv->dev, opm_notify, NULL);
}
DECLARE_HOOK(HOOK_INIT, eppm_init, HOOK_PRIO_DEFAULT);
