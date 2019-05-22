/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Type-C Utils module */

#ifndef __CROS_EC_USBC_UTILS_H
#define __CROS_EC_USBC_UTILS_H

/**
 * Returns the polarity of a Sink.
 *
 * @param cc1 value of CC1 set by tcpm_get_cc
 * @param cc2 value of CC2 set by tcpm_get_cc
 * @return 0 if cc1 is connected, else 1 for cc2
 */
enum pd_cc_polarity_type get_snk_polarity(int cc1, int cc2);

/**
 * Sets the polarity of the port
 *
 * @param port USB-C port number
 * @param polarity 0 for CC1, else 1 for CC2
 */
void set_polarity(int port, int polarity);

#endif /* __CROS_EC_USBC_UTILS_H */

