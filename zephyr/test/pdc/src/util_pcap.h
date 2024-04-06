/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TEST_PDC_SRC_UTIL_PCAP_H
#define TEST_PDC_SRC_UTIL_PCAP_H

#include <stdio.h>

FILE *pcap_open(void);
void pcap_append(FILE *fp, const void *pl, size_t pl_sz);

#endif /* TEST_PDC_SRC_UTIL_PCAP_H */
