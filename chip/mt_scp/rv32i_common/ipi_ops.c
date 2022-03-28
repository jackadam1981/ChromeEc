/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "registers.h"
#include "ipi_chip.h"

__overridable  void ipi_op_wake_ap(void)
{
	SCP_SCP2SPM_IPC_SET = IPC_SCP2HOST;
}

__overridable int ipi_op_scp2ap_is_irq_set(void)
{
	return SCP_SCP2APMCU_IPC_SET & IPC_SCP2HOST;
}

__overridable void ipi_op_scp2ap_irq_set(void)
{
	SCP_SCP2APMCU_IPC_SET = IPC_SCP2HOST;
}

__overridable void ipi_op_ap2scp_irq_clr(void)
{
	SCP_GIPC_IN_CLR = GIPC_IN(0);
}

__overridable int ipi_op_ap2scp_is_irq_set(void)
{
	return SCP_GIPC_IN_SET & GIPC_IN(0);
}

__overridable int ipi_op_core2core(void)
{
	if (SCP_GIPC_IN_SET & SCP_GIPC_IS_CORE0_OK) {
		SCP_GIPC_IN_CLR = SCP_GIPC_IS_CORE0_OK;
	}

	return 0;
}
