/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "task.h"
#include "usb_pd.h"
#include "usb_sm.h"
#include "util.h"
#include "console.h"

static void call_entry_methods(int port, struct usb_state *current) {
	if (!current)
		return;
	
	call_entries_methods(port, current->parent);

	if (current->entry)
		current->entry(port);
}

static void call_exit_methods(int port, struct usb_state *current) {
	if (!current)
		return;

	if (current->exit)
		current->exit(port);

	call_exit_methods(port, current->parent);
}

static void call_run_methods(int port, struct usb_state *current) {
	if (!current)
		return;

	if (current->run)
		current->run(port);

	call_run_methods(port, current->parent);
}

/* No need for init_state, set_state handles it if current == NULL */
void set_state(int port, struct cm_ctx *ctx, struct usb_state *new_state)
{
	call_exit_methods(ctx->current);

	ctx->previous = ctx->current;
	ctx->current = new_state;

	call_entries_methods(ctx->current);
}
void exe_state(int port, struct sm_ctx *ctx)
{
	call_run_methods(port, ctx->current);
}


