/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "registers.h"
#include "ipi_chip.h"

void ipi_op_wake_ap(void)
{
	SCP_SCP2SPM_IPC_SET = IPC_SCP2HOST;
}

int ipi_op_scp2ap_is_irq_set(void)
{
#ifdef MTK_SECURE_SCP
	return SCP_MBOX_OUT_SET(0) & IPC_SCP2HOST;
#else
	return SCP_SCP2APMCU_IPC_SET & IPC_SCP2HOST;
#endif
}

void ipi_op_scp2ap_irq_set(void)
{
#ifdef MTK_SECURE_SCP
	SCP_MBOX_OUT_SET(0) = IPC_SCP2HOST;
#else
	SCP_SCP2APMCU_IPC_SET = IPC_SCP2HOST;
#endif
}

void ipi_op_ap2scp_irq_clr(void)
{
#ifdef MTK_SECURE_SCP
	SCP_MBOX_IN_CLR(0) = GIPC_IN(0);
#else
	SCP_GIPC_IN_CLR = GIPC_IN(0);
#endif
}

int ipi_op_ap2scp_is_irq_set(void)
{
#ifdef MTK_SECURE_SCP
	return SCP_MBOX_IN_SET(0) & GIPC_IN(0);
#else
	return SCP_GIPC_IN_SET & GIPC_IN(0);
#endif
}
