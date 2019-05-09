/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "hotword_dsp_api.h"

const int kGoogleHotwordRequiredDataAlignment = 4;

int GoogleHotwordDspInit(void *hotword_memmap)
{
	DBG("%s: dummy implementation", __func__);
	return 1;
}

int GoogleHotwordDspProcess(const void *samples, int num_samples,
			    int *preamble_length_ms)
{
	DBG("%s: dummy implementation", __func__);
	return 0;
}

void GoogleHotwordDspReset(void)
{
	DBG("%s: dummy implementation", __func__);
}

int GoogleHotwordDspGetMaximumAudioPreambleMs(void)
{
	DBG("%s: dummy implementation", __func__);
	return 0;
}

int GoogleHotwordVersion(void)
{
	DBG("%s: dummy implementation", __func__);
	return 0;
}
