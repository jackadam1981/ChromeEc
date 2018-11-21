/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inter-Processor Interrupt (IPI) */

#include "console.h"
#include "hooks.h"
#include "ipi_chip.h"
#include "system.h"
#include "task.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_IPI, format, ##args)
#define CPRINTS(format, args...) cprints(CC_IPI, format, ##args)

static volatile uint32_t ipi_count;
static struct ipi_desc_t ipi_desc[IPI_MAX];
static struct ipc_desc_t ipc_desc[IPC_MAX];
static struct ipc_share_obj *scp_send_obj, *scp_rcv_obj;

/* Check if SCP to AP IPI is in use. */
static inline int is_ipi_busy(void)
{
	return SCP_HOST_INT & IPC_SCP2HOST_BIT;
}

/* Register a IPC handler. */
void request_ipc(uint32_t ipc_id, ipc_handler_t handler)
{
	ipc_desc[ipc_id].handler = handler;
}

void ipi_wakeup_ap_registration(enum ipi_id id)
{
	ipi_desc[id].is_wakeup_src = 1;
}

inline void ipi_scp2spm(void)
{
	/* Wake APMCU up. */
	SCP_SPM_INT = SPM_INT_A2SPM;
}

/* If id is a wakeup ipi, request SPM to wakeup AP. */
static void try_to_wakeup_ap(enum ipi_id id)
{
	if (ipi_desc[id].is_wakeup_src)
		ipi_scp2spm();
}

/* Send IPI to AP. */
static void ipi_scp2host(enum ipi_id id)
{
	try_to_wakeup_ap(id);
	SCP_HOST_INT = IPC_SCP2HOST_BIT;
}

int ipi_register(enum ipi_id id, ipi_handler_t handler)
{
	ipi_desc[id].handler = handler;

	return EC_SUCCESS;
}

int ipi_unregister(enum ipi_id id)
{
	ipi_desc[id].handler = NULL;

	return EC_SUCCESS;
}

/* Send data from SCP to AP. */
int ipi_send(enum ipi_id id, void *buf, uint32_t len, int wait)
{
	uint32_t ipi_idx;

	/*
	 * TODO(b:117917141): Figure out if this should be call from an
	 * interrupt context.
	 */
	if (wait && in_interrupt_context())
		/* Prevent from infinity wait when be in ISR context. */
		return EC_ERROR_BUSY;

	if (len > sizeof(scp_send_obj->share_buf) || buf == NULL)
		return EC_ERROR_INVAL;

	/*
	 * TODO(b:117917141): Figure out if we really need to stop all the
	 * interrupts here.
	 */
	interrupt_disable();

	/* Check if there is already an IPI pending in AP. */
	if (is_ipi_busy()) {
		/*
		 * If the following conditions meet,
		 *   1) There is an IPI pending in AP.
		 *   2) The incoming IPI is a wakeup IPI.
		 * then it assumes that AP is in suspend state.
		 * Send a AP wakeup request to SPM.
		 *
		 * The incoming IPI will be checked if it's a wakeup source.
		 */
		try_to_wakeup_ap(id);
		interrupt_enable();

		return EC_ERROR_BUSY;
	}

	scp_send_obj->id = id;
	scp_send_obj->len = len;
	memcpy((void *)scp_send_obj->share_buf, buf, len);
	ipi_scp2host(id);

	ipi_idx = ++ipi_count;

	interrupt_enable();

	/*
	 * TODO(b:117917141): Whether if we can use an interrupt, instead of
	 * busy looping.
	 */
	while (wait && is_ipi_busy() && ipi_idx == ipi_count)
		;

	return EC_SUCCESS;
}

static void ipi_handler(void)
{
	if (scp_rcv_obj->id >= IPI_MAX) {
		CPRINTS("#ERR IPI %d", scp_rcv_obj->id);
		return;
	}

	if (!ipi_desc[scp_rcv_obj->id].handler)
		return;

	ipi_desc[scp_rcv_obj->id].handler(
		scp_rcv_obj->id, scp_rcv_obj->share_buf, scp_rcv_obj->len);
}

/* TODO(b:117917141): This should be renamed to something else...  */
void board_ipi_init(void)
{
	struct scp_run_t scp_run;
	int ret;

	scp_run.signaled = 1;
	strncpy(scp_run.fw_ver, system_get_version(SYSTEM_IMAGE_RW),
		SCP_FW_VERSION_LEN);
	scp_run.dec_capability = 0;
	scp_run.enc_capability = 0;

	ret = ipi_send(IPI_SCP_INIT, (void *)&scp_run, sizeof(scp_run), 0);

	if (ret)
		ccprintf("Failed to send initialization IPC messages.\n");
}

static void ipi_host_command_handler(int id, void *share_buf, unsigned int len)
{
	CPRINTS("Recieved Host Command ID=%d 0x%x len=%d", id, share_buf, len);
}

/* Initialize IPI. */
static void ipi_init(void)
{
	scp_send_obj = (struct ipc_share_obj *)CONFIG_IPC_SHARE_BUFFER_ADDR;
	scp_rcv_obj = scp_send_obj + 1;

	memset(scp_send_obj, 0, sizeof(struct ipc_share_obj));

	/* TODO(yllin): When should we clear this IPC register? */
	//SCP_GIPC_IN = SCP_GIPC_IN_CLEAR_IPCN(0);
	request_ipc(IPC_ID(0), ipi_handler);

	/* Register IPI handlers. */
	ipi_register(IPI_HOST_COMMAND, ipi_host_command_handler);

	/* Enable IRQs */
	task_enable_irq(SCP_IRQ_IPC0);
	task_enable_irq(SCP_IRQ_INFRA);

	/* Inform AP that SCP is inited.  */
	board_ipi_init();
	CPRINTS("ipi init");
}
DECLARE_HOOK(HOOK_INIT, ipi_init, HOOK_PRIO_DEFAULT);

void ipc_handler(void)
{
	int i;

	/* Traverse backward. Higher IPC has higher priority. */
	for (i = IPC_MAX - 1; i >= IPC_ID(0); --i) {
		/* Check which IPC has been invoked by AP. */
		if (!(SCP_GIPC_IN & SCP_GIPC_IN_CLEAR_IPCN(i)))
			continue;

		if (ipc_desc[i].handler)
			ipc_desc[i].handler();

		/* SCP write 1 to Clear. */
		SCP_GIPC_IN = SCP_GIPC_IN_CLEAR_IPCN(i);
	}
}
DECLARE_IRQ(SCP_IRQ_IPC0, ipc_handler, 8);

/* AP wakeup SCP and keep SCP awake. */
void scp_infra_irq_handler(void)
{
	CPRINTS("infra_irq_handler");
	/* TODO(b/117917141): Porting infra IRQ handler. */
}
DECLARE_IRQ(SCP_IRQ_INFRA, scp_infra_irq_handler, 8);
