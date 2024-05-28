/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/ztest.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

void test_main(void)
{
	int fd = open("flash.bin", O_RDWR);
	void *ptr = mmap((void *)CONFIG_PLATFORM_EC_MAPPED_STORAGE_BASE,
			 1048576, PROT_WRITE | PROT_READ,
			 MAP_SHARED | MAP_FIXED_NOREPLACE, fd, 0);

	ztest_run_all(NULL, false, 1, 1);

	munmap(ptr, 1048576);
	close(fd);
}
