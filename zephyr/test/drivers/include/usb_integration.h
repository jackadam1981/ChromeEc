/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_INCLUDE_USB_INTEGRATION_H_
#define ZEPHYR_TEST_DRIVERS_INCLUDE_USB_INTEGRATION_H_

#define USB_SINK_PORT USBC_PORT_C0
#define USB_SOURCE_PORT USBC_PORT_C1

struct integration_usb_fixture {
	const struct emul *source;
	const struct emul *sink;
	const struct emul *charger;
};

void *integration_usb_setup(void);

void integration_usb_reset(struct integration_usb_fixture *fixture);

void integration_usb_sink_attach(struct integration_usb_fixture *fixture);
void integration_usb_sink_detach(struct integration_usb_fixture *fixture);

void integration_usb_source_attach(struct integration_usb_fixture *fixture);
void integration_usb_source_detach(struct integration_usb_fixture *fixture);

void integration_usb_get_pd_power_info(
	int port, struct ec_response_usb_pd_power_info *response);

#endif /* ZEPHYR_TEST_DRIVERS_INCLUDE_USB_INTEGRATION_H_ */
