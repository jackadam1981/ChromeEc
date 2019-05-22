/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tcpm.h"
#include "usb_pd.h"

/*
 * CC values for regular sources and Debug sources (aka DTS)
 *
 * Source type  Mode of Operation   CC1    CC2
 * ---------------------------------------------
 * Regular      Default USB Power   RpUSB  Open
 * Regular      USB-C @ 1.5 A       Rp1A5  Open
 * Regular      USB-C @ 3 A         Rp3A0  Open
 * DTS          Default USB Power   Rp3A0  Rp1A5
 * DTS          USB-C @ 1.5 A       Rp1A5  RpUSB
 * DTS          USB-C @ 3 A         Rp3A0  RpUSB
 */

inline enum pd_cc_polarity_type get_snk_polarity(int cc1, int cc2)
{
	/* the following assumes:
	 * TYPEC_CC_VOLT_RP_3_0 > TYPEC_CC_VOLT_RP_1_5
	 * TYPEC_CC_VOLT_RP_1_5 > TYPEC_CC_VOLT_RP_DEF
	 * TYPEC_CC_VOLT_RP_DEF > TYPEC_CC_VOLT_OPEN
	 */
	return (cc2 > cc1) ? POLARITY_CC2 : POLARITY_CC1;
}

void set_polarity(int port, int polarity)
{
	tcpm_set_polarity(port, polarity);
#ifdef CONFIG_USBC_PPC_POLARITY
	ppc_set_polarity(port, polarity);
#endif /* defined(CONFIG_USBC_PPC_POLARITY) */
}
