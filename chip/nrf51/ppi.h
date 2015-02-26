/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * PPI channels are a way to connect NRF51 EVENTs to TASKs without software
 * involvement. They are like SHORTs, except between peripherals.
 */
#include "registers.h"

#define NRF51_PPI_FIRST_PP_CH NRF51_PPI_CH_TIMER0_CC0__RADIO_TXEN
#define NRF51_PPI_LAST_PP_CH  NRF51_PPI_CH_RTC0_COMPARE0__TIMER0_START

int ppi_request_pre_programmed_channel(int ppi_chan);

int ppi_request_channel(int *ppi_chan);

void ppi_release_channel(int ppi_chan);

int ppi_request_group(int *ppi_group);

void ppi_release_group(int ppi_group);
