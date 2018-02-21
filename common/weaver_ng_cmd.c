/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Bridge between weaver_ng and the TPM spec's vendor specific commands. */

#include "weaver_ng.h"

#include "extension.h"
#include "tpm_vendor_cmds.h"

/*
 * Handle the VENDOR_CC_WEAVER_NG command.
 */
static enum vendor_cmd_rc wng_vendor_specific_command(enum vendor_cmd_cc code,
						      void *buf,
						      size_t input_size,
						      size_t *response_size)
{
	static merkle_tree_t merkle_tree;
	const wng_request_t * request = buf;
	wng_response_t * response = buf;
	int ret;

	if (code != VENDOR_CC_WEAVER_NG)
		return VENDOR_RC_BOGUS_ARGS;

	if (input_size != request->header.data_length + sizeof(request->header))
		return VENDOR_RC_REQUEST_TOO_BIG;

	/* TODO(allenwebb) load merkle_tree from flash here or in a startup
	 * hook.
	 */

	ret = wng_handle_request(&merkle_tree, request, response);

	/* TODO(allenwebb) store merkle_tree log update to flash here. */

	*response_size = response->header.data_length +
			sizeof(response->header);

	return ret == EC_SUCCESS ? VENDOR_RC_SUCCESS : VENDOR_RC_INTERNAL_ERROR;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_WEAVER_NG,
		       wng_vendor_specific_command);
