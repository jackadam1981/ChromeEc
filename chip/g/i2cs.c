/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "pmu.h"
#include "registers.h"
#include "task.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

static void i2cs_init(void)
{
	/* First decide if i2c is even needed for this platform. */

	/* if (i2cs is not needed) return; */

	pmu_clock_en(PERIPH_I2CS);

	/*
	 * i2cs function has been already configured in the gpio.inc table,
	 * here just enable pull ups on both signals.
	 */
	GWRITE_FIELD(PINMUX, DIOB0_CTL, PU, 1);
	GWRITE_FIELD(PINMUX, DIOB0_CTL, PU, 1);

	GWRITE_FIELD(I2CS, INT_ENABLE, INTR_READ_BEGIN, 1);
	GWRITE_FIELD(I2CS, INT_ENABLE, INTR_READ_COMPLETE, 1);
	GWRITE_FIELD(I2CS, INT_ENABLE, INTR_WRITE_COMPLETE, 1);

	GWRITE(I2CS, SLAVE_DEVADDRVAL, 0x50);
}
DECLARE_HOOK(HOOK_INIT, i2cs_init, HOOK_PRIO_DEFAULT);

void _i2cs_read_begin_int(void)
{
	CPRINTS("%s:%d\n", __func__, __LINE__);
	GWRITE_FIELD(I2CS, INT_STATE, INTR_READ_BEGIN, 1);
}

void _i2cs_read_complete_int(void)
{
	CPRINTS("%s:%d\n", __func__, __LINE__);
	GWRITE_FIELD(I2CS, INT_STATE, INTR_READ_COMPLETE, 1);
}

void _i2cs_write_complete_int(void)
{
	CPRINTS("%s:%d\n", __func__, __LINE__);
	GWRITE_FIELD(I2CS, INT_STATE, INTR_WRITE_COMPLETE, 1);
}


DECLARE_IRQ(GC_IRQNUM_I2CS0_INTR_READ_BEGIN_INT, _i2cs_read_begin_int, 1);
DECLARE_IRQ(GC_IRQNUM_I2CS0_INTR_READ_COMPLETE_INT, _i2cs_read_complete_int, 1);
DECLARE_IRQ(GC_IRQNUM_I2CS0_INTR_WRITE_COMPLETE_INT,
	    _i2cs_write_complete_int, 1);
