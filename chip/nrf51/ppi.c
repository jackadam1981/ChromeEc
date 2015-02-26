/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppi.h"
#include "registers.h"
#include "util.h"

static uint32_t channels_in_use;
static uint32_t channel_groups_in_use;

int ppi_request_pre_programmed_channel(int ppi_chan)
{
	ASSERT(ppi_chan >= NRF51_PPI_FIRST_PP_CH &&
		ppi_chan <= NRF51_PPI_LAST_PP_CH);

	if (channels_in_use & (1 << ppi_chan))
		return EC_ERROR_BUSY;

	channels_in_use |= (1 << ppi_chan);

	return EC_SUCCESS;
}

int ppi_request_channel(int *ppi_chan)
{
	int chan;

	for (chan = 0; chan < 16; chan++)
		if ((channels_in_use & (1 << chan)) == 0)
			break;

	if (chan == 16)
		return EC_ERROR_BUSY;

	channels_in_use |= (1 << chan);
	*ppi_chan = chan;
	return EC_SUCCESS;
}

void ppi_release_channel(int ppi_chan)
{
	channels_in_use &= ~(1 << ppi_chan);
}

void ppi_release_group(int ppi_group)
{
	channel_groups_in_use &= ~(1 << ppi_group);
}

int ppi_request_group(int *ppi_group)
{
	int group;

	for (group = 0; group < 4; group++)
		if ((channel_groups_in_use & (1 << group)) == 0)
			break;

	if (group == 4)
		return EC_ERROR_BUSY;

	channel_groups_in_use |= (1 << group);
	*ppi_group = group;
	return EC_SUCCESS;
}
