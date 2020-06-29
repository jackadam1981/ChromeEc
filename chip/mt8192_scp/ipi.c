/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "registers.h"
#include "ipi_chip.h"
#include "util.h"
#include "system.h"

static struct ipc_shared_obj *const scp_send_obj =
	(struct ipc_shared_obj *)CONFIG_IPC_SHARED_OBJ_ADDR;
#if 0
static struct ipc_shared_obj *const scp_recv_obj =
	(struct ipc_shared_obj *)(CONFIG_IPC_SHARED_OBJ_ADDR +
				  sizeof(struct ipc_shared_obj));
#endif

/* Send data from SCP to AP. */
int ipi_send(int32_t id, const void *buf, uint32_t len, int wait)
{
	if (len > sizeof(scp_send_obj->buffer))
		return EC_ERROR_INVAL;

	scp_send_obj->id = id;
	scp_send_obj->len = len;
	memcpy(scp_send_obj->buffer, buf, len);

	/* Send IPI to AP: interrutp AP to receive IPI messages. */
	SCP_SCP2APMCU_IPC_SET = IPC_SCP2HOST;
	
	//try_to_wakeup_ap(id);

	return EC_SUCCESS;
}


static void ipi_enable_ipc0_deferred(void)
{
	struct scp_run_t scp_run;
	int ret;

	scp_run.signaled = 1;
	strncpy(scp_run.fw_ver, system_get_version(EC_IMAGE_RW),
		SCP_FW_VERSION_LEN);
	scp_run.dec_capability = VCODEC_CAPABILITY_4K_DISABLED;
	scp_run.enc_capability = 0;

	ret = ipi_send(IPI_SCP_INIT, (void *)&scp_run, sizeof(scp_run), 1);
	if (ret)
		ccprints("failed to send initialization IPC messages.\n");

	ccprints("ipi init");
}
DECLARE_DEFERRED(ipi_enable_ipc0_deferred);

/* Initialize IPI. */
static void ipi_init(void)
{
	/* Clear send share buffer. */
	memset(scp_send_obj, 0, sizeof(struct ipc_shared_obj));

	/* Enable IRQ after all tasks are up.  */
	hook_call_deferred(&ipi_enable_ipc0_deferred_data, 0);
}
DECLARE_HOOK(HOOK_INIT, ipi_init, HOOK_PRIO_DEFAULT);
