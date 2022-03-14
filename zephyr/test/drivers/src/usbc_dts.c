/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>

#include "driver/bc12/pi3usb9201_public.h"
#include "driver/ppc/sn5s330_public.h"
#include "driver/ppc/syv682x_public.h"
#include "usb_charge.h"
#include "usbc_ppc.h"

ZTEST(usbc_dts, test_bc12_ports)
{
	zassert_equal(bc12_ports[0].drv, &pi3usb9201_drv, NULL);
	zassert_equal(pi3usb9201_bc12_chips[0].i2c_port, I2C_PORT_USB_C0, NULL);
	zassert_equal(pi3usb9201_bc12_chips[0].i2c_addr_flags,
			PI3USB9201_I2C_ADDR_3_FLAGS, NULL);

	zassert_equal(bc12_ports[1].drv, &pi3usb9201_drv, NULL);
	zassert_equal(pi3usb9201_bc12_chips[1].i2c_port, I2C_PORT_USB_C1, NULL);
	zassert_equal(pi3usb9201_bc12_chips[1].i2c_addr_flags,
			PI3USB9201_I2C_ADDR_1_FLAGS, NULL);
}

ZTEST(usbc_dts, test_ppc_chips)
{
	zassert_equal(ppc_cnt, 2, "ppc_cnt should be 2");

	zassert_equal(ppc_chips[0].i2c_port, I2C_PORT_USB_C0, NULL);
	zassert_equal(ppc_chips[0].i2c_addr_flags, SN5S330_ADDR0_FLAGS, NULL);
	zassert_equal(ppc_chips[0].drv, &sn5s330_drv, NULL);

	zassert_equal(ppc_chips[1].i2c_port, I2C_PORT_USB_C1, NULL);
	zassert_equal(ppc_chips[1].i2c_addr_flags, SYV682X_ADDR1_FLAGS, NULL);
	zassert_equal(ppc_chips[1].drv, &syv682x_drv, NULL);
}

ZTEST_SUITE(usbc_dts, NULL, NULL, NULL, NULL, NULL);
