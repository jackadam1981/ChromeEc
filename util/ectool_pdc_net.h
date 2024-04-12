/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ECTOOL_PDC_NET_H
#define ECTOOL_PDC_NET_H

#include <stddef.h>

#define USB_PDC_UDP_PORT 2012

int pdc_net_open(const char *hostname);
void pdc_net_out(const void *buf, size_t buf_len);
void pdc_net_close(void);

const char *pdc_net_get_vm_ip(void);

#endif /* ECTOOL_PDC_NET_H */
