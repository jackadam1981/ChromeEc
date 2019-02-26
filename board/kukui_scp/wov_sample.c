/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kukui SCP wake on voice sample */

#include "assert.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "wov_chip.h"

#define AUDIO_BUF_SIZE (32768)
static uint16_t audio_buffer[AUDIO_BUF_SIZE];
static const uint16_t *audio_buffer_end = audio_buffer + AUDIO_BUF_SIZE;

static void wov_event_handler(size_t nsample);
static const struct wov_config wov_conf = {
	.samplerate = WOV_SAMPLERATE_16K,
	.mic_type = WOV_MICTYPE_DMIC,
	.fifo_notify = wov_event_handler,
};

static struct wov_driver const *wov_hw_drv;

/* Initialize wov */
static void board_wov_init(void)
{
	wov_hw_drv = wov_driver_init(&wov_conf);
}

/* Fifo data callback */
static void wov_event_handler(size_t nsample)
{
	/* Process in the interrupt context, or defer it to a task */
	task_wake(TASK_ID_AUDIO);
}

static size_t buffer_size(uint16_t *rp, uint16_t *wp)
{
	return (wp >= rp) ? (wp - rp) : (AUDIO_BUF_SIZE - (rp - wp) + 1);
}
/* Audio task */
void audio_task(void)
{
	uint16_t *rp;
	uint16_t *wp;

	rp = wp = audio_buffer;
	board_wov_init();
	/* Enable FIFO event notification */
	task_enable_irq(SCP_IRQ_MAD_FIFO);
	wov_hw_drv->enable(1);

	while (1) {
		size_t nsample;

		if (!wov_hw_drv) {
			task_wait_event(-1);
			break;
		}

		nsample = wov_hw_drv->get_fifo_level();
		if (!nsample) {
			task_wait_event(-1);
			break;
		}

		/* Buffer full */
		if (buffer_size(rp, wp) >= AUDIO_BUF_SIZE) {
			rp += nsample;
			if (rp >= audio_buffer_end)
				rp -= AUDIO_BUF_SIZE;
			ccprintf("r:%u ", rp - audio_buffer);
		}

		/* Read into buffer */
		wp += wov_hw_drv->read_fifo(wp,
				((wp + nsample) < audio_buffer_end) ?
				nsample : (audio_buffer - wp));
		ccprintf("w:%u ", wp - audio_buffer);
	}
}
