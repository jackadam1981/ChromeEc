/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/espi.h>
#include <logging/log.h>
#include <kernel.h>

LOG_MODULE_REGISTER(cros_shi_espi, CONFIG_ESPI_LOG_LEVEL);


#if DT_HAS_COMPAT_STATUS_OKAY(cros_shi_espi)

#define GEN_CFG(id)						\
{								\
	.dev = DEVICE_DT_GET(DT_PHANDLE(id, device)),		\
	.cfg = {						\
		.max_freq = DT_PROP(id, frequency),		\
		.io_caps = DT_PROP(id, io_caps),		\
		.channel_caps = DT_PROP(id, channel_caps),	\
	},							\
},

static int espi_init(const struct device *unused)
{
	struct {
		const struct device *dev;
		struct espi_cfg cfg;
	} cfg_list[] = {
		DT_FOREACH_STATUS_OKAY(cros_shi_espi, GEN_CFG)
	};

	for (int i = 0; i < ARRAY_SIZE(cfg_list); i++) {
		const struct device *dev = cfg_list[i].dev;

		if (!device_is_ready(dev)) {
			k_oops();
		}

		/* Configure eSPI */
		if (espi_config(dev, &cfg_list[i].cfg)) {
			LOG_ERR("Failed to configure eSPI device: %s",
				dev->name);
		}
	}
	return 0;
}
SYS_INIT(espi_init, APPLICATION, 0);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(cros_shi_espi) */
