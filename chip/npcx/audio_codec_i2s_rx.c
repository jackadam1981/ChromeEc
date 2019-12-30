/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "audio_codec.h"
#include "ec_commands.h"
#include "wov_chip.h"

#define CPRINTS(format, args...) cprints(CC_AUDIO_CODEC, format, ## args)

int audio_codec_i2s_rx_enable()
{
	wov_set_mic_source(WOV_SRC_STEREO);
	wov_set_mode(WOV_MODE_OFF);
	wov_set_sample_rate(48000);
	return wov_set_mode(WOV_MODE_I2S);
}

int audio_codec_i2s_rx_disable()
{
	return wov_set_mode(WOV_MODE_OFF);
}

int audio_codec_i2s_rx_set_sample_depth(uint8_t depth)
{
	int _depth;

	if (depth == EC_CODEC_I2S_RX_SAMPLE_DEPTH_24)
		_depth = 24;
	else
		_depth = 16;

	return wov_set_sample_depth(_depth);
}

int audio_codec_i2s_rx_set_daifmt(uint8_t daifmt)
{
	enum wov_dai_format _daifmt = WOV_DAI_FMT_I2S;

	switch (daifmt) {
	case EC_CODEC_I2S_RX_DAIFMT_I2S:
		_daifmt = WOV_DAI_FMT_I2S;
		break;
	case EC_CODEC_I2S_RX_DAIFMT_RIGHT_J:
		_daifmt = WOV_DAI_FMT_RIGHT_J;
		break;
	case EC_CODEC_I2S_RX_DAIFMT_LEFT_J:
		_daifmt = WOV_DAI_FMT_LEFT_J;
		break;
	}

	wov_set_mode(WOV_MODE_OFF);
	wov_set_i2s_fmt(_daifmt);

	return EC_SUCCESS;
}

int audio_codec_i2s_rx_set_bclk(uint32_t bclk)
{
	wov_set_mode(WOV_MODE_OFF);
	wov_set_i2s_bclk(bclk);
	return EC_SUCCESS;
}
