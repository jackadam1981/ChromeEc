/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpc_bep_bio_alg.h"

#include <errno.h>

int bio_algorithm_init(void) {
	return 0;
}

int bio_algorithm_exit(void) {
	return 0;
}

int bio_enrollment_begin(bio_enrollment_t *enrollment) {
	return -ENOTSUP;
}

int bio_enrollment_finish(bio_enrollment_t enrollment, bio_template_t *templ) {
	return -ENOTSUP;
}
