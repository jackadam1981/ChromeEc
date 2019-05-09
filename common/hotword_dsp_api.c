/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hotword_dsp_api.h"

__attribute__((weak))
const int kGoogleHotwordRequiredDataAlignment = 4;

__attribute__((weak))
int GoogleHotwordDspInit(void *hotword_memmap)
{
	return 1;
}

__attribute__((weak))
int GoogleHotwordDspProcess(const void *samples, int num_samples,
			    int *preamble_length_ms)
{
	return 0;
}

__attribute__((weak))
void GoogleHotwordDspReset(void)
{
}

__attribute__((weak))
int GoogleHotwordDspGetMaximumAudioPreambleMs(void)
{
	return 0;
}

__attribute__((weak))
int GoogleHotwordVersion(void)
{
	return 0;
}
