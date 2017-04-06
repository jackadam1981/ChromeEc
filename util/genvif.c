/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "usb_pd.h"

char *yes_no(int val)
{
	return val ? "YES" : "NO";
}

#ifdef CONFIG_USB_PD_DYNAMIC_SRC_CAP
static char *is_extpwr(void)
{
	return "#UNKNOWN";
}
#else
/* From board usb_pd_policy.c */
extern const uint32_t pd_src_pdo[];
extern const int pd_src_pdo_cnt;

static char *is_extpwr(void)
{
	return yes_no(pd_src_pdo[0] & PDO_FIXED_EXTERNAL);
}
#endif

#ifdef CONFIG_USB_PD_GIVE_BACK
const int giveback = 1;
#else
const int giveback = 0;
#endif

int main(int argc, char **argv)
{
#ifdef CONFIG_USB_PID
	printf("PID_SOP: 0x%04x\n", CONFIG_USB_PID); 
#endif /* CONFIG_USB_PID */
	printf("Externally_Powered: %s\n", is_extpwr());
	printf("GiveBack_May_Be_Set: %s\n", yes_no(giveback));

	return 0;
}
