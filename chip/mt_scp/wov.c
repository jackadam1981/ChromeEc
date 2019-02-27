/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util.h"
#include "console.h"
#include "link_defs.h"
#include "memmap.h"
#include "hooks.h"
#include "registers.h"
#include "wov.h"

#ifdef DEBUG_WOV
#define CPRINTF(format, args...) cprintf(CC_WOV, format, ##args)
#else
#define CPRINTF(format, args...)
#endif /* DEBUG_WOV */

static void (*notify)(void *priv_data);
static void *notify_data;

#ifndef CONFIG_EC_CODEC_WOV_CAP_AUDIO_SHM
static uint8_t audio_buf[65536];
#endif

#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
__SECTION(dram)
#endif
static uint8_t lang_buf[40 * 1024];

/* VIF FIFO irq is triggered above this level */
#define WOV_TRIGGER_LEVEL 160

static int wov_enable(void)
{
	uint32_t serial_if_cfg0 = RXIF_CFG0_RESET_VAL;

	/* Reset FIFO and stop IRQ */
	SCP_VIF_FIFO_EN &= ~(VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN);

	/* WOV_MICTYPE_DMIC */
	serial_if_cfg0 |= RXIF_RGDL2_DMIC_16K;

	SCP_RXIF_CFG0 = serial_if_cfg0;
	SCP_RXIF_CFG1 = RXIF_CFG1_RESET_VAL;

	SCP_VIF_FIFO_EN |= VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN;
	SCP_VIF_FIFO_DATA_THRE = WOV_TRIGGER_LEVEL + 1;

	task_enable_irq(SCP_IRQ_MAD_FIFO);

	return EC_SUCCESS;
}

static int wov_disable(void)
{
	/* Reset FIFO and stop IRQ */
	SCP_VIF_FIFO_EN &= ~(VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN);

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
	uint16_t *out = buf;

	count >>= 1;

	while (count-- && wov_fifo_level())
		*out++ = SCP_VIF_FIFO_DATA;

	return (void *)out - buf;
}

static void wov_set_callback(void (*cb)(void *priv_data), void *priv_data)
{
	notify = cb;
	notify_data = priv_data;
}

void wov_fifo_interrupt_handler(void)
{
	if (notify)
		notify(notify_data);

	/* Read to clear */
	SCP_VIF_FIFO_IRQ_STATUS;
}
DECLARE_IRQ(SCP_IRQ_MAD_FIFO, wov_fifo_interrupt_handler, 2);

static struct wov_driver wov_driver = {
	.enable = wov_enable,
	.disable = wov_disable,
	.read = wov_read,
	.set_callback = wov_set_callback,

#ifdef CONFIG_EC_CODEC_WOV_CAP_AUDIO_SHM
	.audio_buf_addr = CONFIG_AUDIO_SHM_BASE,
	.audio_buf_len = CONFIG_AUDIO_SHM_SIZE,
	.audio_buf_type = EC_CODEC_WOV_SHM_TYPE_EC_RAM,
#else
	.audio_buf_addr = (uintptr_t)audio_buf,
	.audio_buf_len = ARRAY_SIZE(audio_buf),
#endif

	.lang_buf_addr = (uintptr_t)lang_buf,
	.lang_buf_len = ARRAY_SIZE(lang_buf),
#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
	.lang_buf_type = EC_CODEC_WOV_SHM_TYPE_SYSTEM_RAM,
#endif
};

static void wov_init(void)
{
#ifdef CONFIG_EC_CODEC_WOV_CAP_LANG_SHM
	{
		uintptr_t ap_addr;

		memmap_scp_cache_to_ap((uintptr_t)lang_buf, &ap_addr);
		wov_driver.lang_buf_addr = ap_addr;
	}
#endif

	if (wov_register_driver(&wov_driver) != EC_SUCCESS)
		ccprintf("failed to register wov_driver\n");
}
DECLARE_HOOK(HOOK_INIT, wov_init, HOOK_PRIO_DEFAULT);
