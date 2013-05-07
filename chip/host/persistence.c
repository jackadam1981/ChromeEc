/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Persistence module for emulator */

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 1024

void get_storage_path(char *out)
{
	int sz = readlink("/proc/self/exe", out, BUF_SIZE);
	while (out[--sz] != '/')
		;
	out[sz] = '\0';
}

FILE *get_persistent_storage(const char *tag, const char *mode)
{
	char buf[BUF_SIZE];
	char path[BUF_SIZE];

	get_storage_path(buf);
	sprintf(path, "%s/persist_%s", buf, tag);

	return fopen(path, mode);
}
