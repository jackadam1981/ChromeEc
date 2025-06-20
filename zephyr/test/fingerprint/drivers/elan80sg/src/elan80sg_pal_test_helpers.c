/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "elan80sg_pal_test_helpers.h"

int z_impl_elan80sg_elan_usleep(unsigned int us)
{
	return elan_usleep(us);
}

void *z_impl_elan80sg_elan_malloc(uint32_t size)
{
	return elan_malloc(size);
}

void z_impl_elan80sg_elan_free(void *data)
{
	return elan_free(data);
}
