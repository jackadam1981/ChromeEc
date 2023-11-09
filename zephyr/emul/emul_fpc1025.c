/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/drivers/spi_emul.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT fpc_fpc1025

LOG_MODULE_REGISTER(emul_fpc1025, CONFIG_SPI_LOG_LEVEL);

static int fpc1025_emul_io(const struct emul *target,
			   const struct spi_config *config,
			   const struct spi_buf_set *tx_bufs,
			   const struct spi_buf_set *rx_bufs)
{
}

static struct spi_emul_api fpc1025_emul_api = {
	.io = fpc1025_emul_io,
};

static int fpc1025_emul_init(const struct emul *emul,
			     const struct device *parent)
{
	return 0;
}

#define FPC1025_EMUL(n)                                       \
	EMUL_DT_INST_DEFINE(n, fpc1025_emul_init, NULL, NULL, \
			    &fpc1025_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(FPC1025_EMUL);
