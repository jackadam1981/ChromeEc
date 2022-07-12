/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <atomic.h>
#include <zephyr/drivers/espi.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

#include "signal_vw.h"

#define MY_COMPAT intel_ap_pwrseq_vw

#if HAS_VW_SIGNALS

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

#define INIT_ESPI_SIGNAL(id)                                            \
	{                                                               \
		.espi_signal = DT_STRING_UPPER_TOKEN(id, virtual_wire), \
		.signal = PWR_SIGNAL_ENUM(id),                          \
		.invert = DT_PROP(id, vw_invert),                       \
	},

/*
 * Struct containing the eSPI virtual wire config.
 */
struct vw_config {
	uint8_t espi_signal; /* associated VW signal */
	uint8_t signal; /* power signal */
	bool invert; /* Invert the signal value */
};

const static struct vw_config vw_config[] = { DT_FOREACH_STATUS_OKAY(
	MY_COMPAT, INIT_ESPI_SIGNAL) };

/*
 * Current signal value.
 */
static atomic_t signal_data;
/*
 * Mask of valid signals. A signal is considered valid once an
 * initial value has been received for it.
 */
static atomic_t signal_valid;

#define espi_dev DEVICE_DT_GET(DT_CHOSEN(intel_ap_pwrseq_espi))

BUILD_ASSERT(ARRAY_SIZE(vw_config) <= (sizeof(atomic_t) * 8));

static void vw_rx(int index, uint32_t data)
{
	bool value = vw_config[index].invert ? !data : !!data;

	atomic_set_bit_to(&signal_data, index, value);
	atomic_set_bit(&signal_valid, index);
	power_signal_interrupt(vw_config[index].signal, value);
}

/*
 * Receive and process a VW signal.
 * This may be called before power_signal_vw_init() is called, since
 * it is called from the ESPI shim handler, so possibly
 * power_signal_interrupt() will be called before any of the
 * power sequence init is done (this should be fine).
 */
void power_signal_vw_received(enum espi_vwire_signal sig, uint32_t data)
{
	/*
	 * A platform reset asserted signal is handled specially by
	 * resetting all signals to their default (0) values.
	 */
	if (sig == ESPI_VWIRE_SIGNAL_PLTRST) {
		LOG_ERR("PLTRST rx %d", data);
		if (0 && data == 0) {
			for (int i = 0; i < ARRAY_SIZE(vw_config); i++) {
				vw_rx(i, 0);
			}
		}
	} else {
		for (int i = 0; i < ARRAY_SIZE(vw_config); i++) {
			if (sig == vw_config[i].espi_signal) {
				LOG_ERR("RX signal %d value %d", i, data);
				vw_rx(i, data);
			}
		}
	}
}

int power_signal_vw_get(enum pwr_sig_vw vw)
{
	if (vw < 0 || vw >= ARRAY_SIZE(vw_config) ||
	    !atomic_test_bit(&signal_valid, vw)) {
		return -EINVAL;
	}
	return atomic_test_bit(&signal_data, vw);
}

void power_signal_vw_init(void)
{
	/*
	 * Check whether the bus is ready, and if so,
	 * initialise the current values of the signals.
	 * This may not be necessary any more since the
	 * vw handler is now invoked from the shim handler,
	 * and potentially there may be a race condition here.
	 * TODO: Consider removing this.
	 */
	if (espi_get_channel_status(espi_dev, ESPI_CHANNEL_VWIRE)) {
		for (int i = 0; i < ARRAY_SIZE(vw_config); i++) {
			uint8_t vw_value;

			if (espi_receive_vwire(espi_dev,
					       vw_config[i].espi_signal,
					       &vw_value) == 0) {
				LOG_ERR("init signal %d value %d", i, vw_value);
				vw_rx(i, vw_value);
			}
		}
	}
}

#endif /* HAS_VW_SIGNALS */
