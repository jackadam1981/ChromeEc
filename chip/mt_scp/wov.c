/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Audio data FIFO */

#include "common.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "wov_chip.h"

/* VIF FIFO irq is triggered above this level */
#define WOV_TRIGGER_LEVEL 160

static struct wov_config const *wov_conf;

static void wov_enable_fifo(int en)
{
	uint32_t serial_if_cfg0 = RXIF_CFG0_RESET_VAL;

	/* Reset FIFO and stop IRQ */
	SCP_VIF_FIFO_EN &= ~(VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN);

	if (!en)
		return;

	if (!wov_conf)
		return;

	if (wov_conf->samplerate == WOV_SAMPLERATE_32K) {
		serial_if_cfg0 |= RXIF_RGDL2_AMIC_32K;
	} else if (wov_conf->samplerate == WOV_SAMPLERATE_16K) {
		/* Serial interface supports different mic type under 16K */
		switch (wov_conf->mic_type) {
		case WOV_MICTYPE_DMIC:
			serial_if_cfg0 |= RXIF_RGDL2_DMIC_16K;
			break;
		case WOV_MICTYPE_DMIC_LP:
			serial_if_cfg0 |= RXIF_RGDL2_DMIC_LP_16K;
			break;
		case WOV_MICTYPE_AMIC:
		default:
			serial_if_cfg0 |= RXIF_RGDL2_AMIC_16K;
		}
	} else {
		return;
	}

	SCP_RXIF_CFG0 = serial_if_cfg0;
	SCP_RXIF_CFG1 = RXIF_CFG1_RESET_VAL;

	SCP_VIF_FIFO_EN |= VIF_FIFO_RSTN | VIF_FIFO_IRQ_EN;
	SCP_VIF_FIFO_DATA_THRE = WOV_TRIGGER_LEVEL + 1;
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

static size_t wov_read_fifo(uint16_t *output_buffer, size_t max_read_size)
{
	size_t read_size = max_read_size;
	uint16_t *out = output_buffer;
	uint16_t sample;

	while (read_size && wov_fifo_level()) {
		sample = SCP_VIF_FIFO_DATA;
		read_size--;
		if (output_buffer)
			*out++ = sample;
	}

	return max_read_size - read_size;
}

static uint32_t wov_irq_ack(void)
{
	/* Read to clear */
	return SCP_VIF_FIFO_IRQ_STATUS;
}

static const struct wov_driver scp_wov_driver = {
	.enable = wov_enable_fifo,
	.get_fifo_level = wov_fifo_level,
	.read_fifo = wov_read_fifo,
};

struct wov_driver const *wov_driver_init(const struct wov_config *cfg)
{
	wov_conf = cfg;
	return &scp_wov_driver;
}

void wov_fifo_interrupt_handler(void)
{
	if (wov_conf && wov_conf->fifo_notify)
		wov_conf->fifo_notify(wov_fifo_level());

	wov_irq_ack();
}
DECLARE_IRQ(SCP_IRQ_MAD_FIFO, wov_fifo_interrupt_handler, 2);
