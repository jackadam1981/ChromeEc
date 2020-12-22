/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel BASEBOARD-RVP MCHP EC specific configuration */

#ifndef __CROS_EC_MCHP_EC_H
#define __CROS_EC_MCHP_EC_H


/* ADC channels */
#if 0
#define ADC_TEMP_SNS_AMBIENT_CHANNEL	CHIP_ADC_CH13
#define ADC_TEMP_SNS_DDR_CHANNEL	CHIP_ADC_CH15
#define ADC_TEMP_SNS_SKIN_CHANNEL	CHIP_ADC_CH6
#define ADC_TEMP_SNS_VR_CHANNEL		CHIP_ADC_CH1
#endif

#ifdef CONFIG_USBC_VCONN
	#define CONFIG_USBC_VCONN_SWAP
	/* delay to turn on/off vconn */
	#define PD_VCONN_SWAP_DELAY 5000 /* us */
#endif
#endif /* __CROS_EC_MCHP_EC_H */
