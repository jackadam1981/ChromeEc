/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef UTIL_PCAP_H
#define UTIL_PCAP_H

#include <stdio.h>

extern FILE *pcap_open(void);
extern void pcap_append(FILE *fp, const void *pl, size_t pl_sz);

#endif /* UTIL_PCAP_H */
