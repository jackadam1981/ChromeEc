/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util.h"
#include "memmap.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"

#include "audio_codec.h"

static void (*read_notifiee)(void *priv_data);
static void *read_notifiee_data;

/* VIF FIFO irq is triggered above this level */
#define WOV_TRIGGER_LEVEL 160

static int wov_enable_notifier(void)
{
	SCP_VIF_FIFO_DATA_THRE = WOV_TRIGGER_LEVEL + 1;
	SCP_VIF_FIFO_EN |= VIF_FIFO_IRQ_EN;

	task_enable_irq(SCP_IRQ_MAD_FIFO);

	return EC_SUCCESS;
}

static int wov_disable_notifier(void)
{
	SCP_VIF_FIFO_EN &= ~VIF_FIFO_IRQ_EN;

	task_disable_irq(SCP_IRQ_MAD_FIFO);

	return EC_SUCCESS;
}

static int wov_enable(void)
{
	uint32_t serial_if_cfg0 = RXIF_CFG0_RESET_VAL;

	SCP_VIF_FIFO_EN = 0;

	/* WOV_MICTYPE_DMIC */
	serial_if_cfg0 |= RXIF_RGDL2_DMIC_16K;

	SCP_RXIF_CFG0 = serial_if_cfg0;
	SCP_RXIF_CFG1 = RXIF_CFG1_RESET_VAL;

	SCP_VIF_FIFO_EN |= VIF_FIFO_RSTN;

	return EC_SUCCESS;
}

static int wov_disable(void)
{
	SCP_VIF_FIFO_EN = 0;

	return EC_SUCCESS;
}

static size_t wov_fifo_level(void)
{
	uint32_t fifo_status = SCP_VIF_FIFO_STATUS;

	if (!(fifo_status & VIF_FIFO_VALID))
		return 0;

	if (fifo_status & VIF_FIFO_FULL)
		return VIF_FIFO_MAX;

	return VIF_FIFO_LEVEL(fifo_status);
}

static int32_t wov_read(void *buf, uint32_t count)
{
	int16_t *out = buf;

	count >>= 1;

	while (count-- && wov_fifo_level())
		*out++ = SCP_VIF_FIFO_DATA;

	return (void *)out - buf;
}

static void wov_set_read_notifiee(void (*cb)(void *priv_data), void *priv_data)
{
	read_notifiee = cb;
	read_notifiee_data = priv_data;
}

static void wov_fifo_interrupt_handler(void)
{
	if (read_notifiee)
		read_notifiee(read_notifiee_data);

	wov_disable_notifier();

	/* Read to clear */
	SCP_VIF_FIFO_IRQ_STATUS;
}
DECLARE_IRQ(SCP_IRQ_MAD_FIFO, wov_fifo_interrupt_handler, 2);

static struct audio_codec_driver driver = {
	.translate_addr_ec_to_ap = memmap_scp_cache_to_ap,
};

static struct audio_codec_wov_driver wov_driver = {
	.enable = wov_enable,
	.disable = wov_disable,
	.read = wov_read,
	.enable_notifier = wov_enable_notifier,
	.disable_notifier = wov_disable_notifier,
	.set_read_notifiee = wov_set_read_notifiee,
};

static void chip_wov_init(void)
{
	if (audio_codec_register_driver(&driver) != EC_SUCCESS)
		ERR("failed to register driver");
	if (audio_codec_register_wov_driver(&wov_driver) != EC_SUCCESS)
		ERR("failed to register wov_driver");

	DBG("%s", __func__);
}
DECLARE_HOOK(HOOK_INIT, chip_wov_init, HOOK_PRIO_DEFAULT);
