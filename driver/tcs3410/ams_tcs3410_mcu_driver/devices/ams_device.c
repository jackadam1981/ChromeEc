/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#include <stdio.h>
#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "hwtimer.h"
#include "math_util.h"
#include "ams_errno.h"
#include "ams_device.h"
#include "tcs3410_hwdef.h"
#include "tcs3410.h"
#include "als_tcs3410.h"

volatile uint32_t last_interrupt_timestamp;

struct ams_device device;

/**
 * Initialise TCS3410 light sensor.
 */
static int tcs3410_rgb_init(struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int tcs3410_init(struct motion_sensor_t *s)
{
    ams_errno_t ret_val = ams_sensor_init(&device);

    if (ret_val == AMS_SUCCESS)
    {
        AMS_LOG_PRINTF(LOG_INFO, "Sensor init success.");
        return EC_SUCCESS;
    }
    else
    {
        AMS_LOG_PRINTF(LOG_ERROR, "Sensor init failed.\n");
    }
    return EC_ERROR_NOT_HANDLED;
}

static int tcs3410_rgb_read(const struct motion_sensor_t *s, intv3_t v)
{
	ccprintf("WARNING: tcs3410_rgb_read() should never be called\n");
	return EC_SUCCESS;
}

static int tcs3410_read(const struct motion_sensor_t *s, intv3_t v)
{
	/*
	 * Read last values.
	 */
	ccprintf("not implemented");
	return EC_SUCCESS;
}


void tcs3410_interrupt(enum gpio_signal signal)
{
	last_interrupt_timestamp = __hw_clock_source_read();

	task_set_event(TASK_ID_MOTIONSENSE, CONFIG_ALS_TCS3410_INT_EVENT);
}

/*
 * Entry point for all interrupts - tcs3410_irq.c module handles
 * the processing of all interrupts.
 */
int tcs3410_irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	if (!(*event & CONFIG_ALS_TCS3410_INT_EVENT))
		return EC_ERROR_NOT_HANDLED;

	device.irq();

	return EC_SUCCESS;
}

static int tcs3410_rgb_set_data_rate(const struct motion_sensor_t *s, int rate,
				     int rnd)
{
	return EC_SUCCESS;
}

static int tcs3410_set_data_rate(const struct motion_sensor_t *s, int rate,
				 int rnd)
{
	ams_sensor_config_t cfg;
	int ret = EC_SUCCESS;

	device.setup(&cfg);
	if (rate == 0) {
		/*
		 * Disable PON to leave device in SLEEP state when it reaches
		 * it.
		 */
		device.pon(false);

		/*
		 * Enable SAI (Sleep on Interrupt), to put the device in sleep
		 * mode.
		 */
		device.sai(AMS_SAI_ENABLE);
	} else {
		/*
		 * See Figure 24 and Figure 52 for atime and wait time between
		 * measurements calculation.
		 */
		cfg.wait_time = FP_TO_INT(fp_div(fp_div(INT_TO_FP(1), INT_TO_FP(rate)) -
					         ((cfg.als_nr_samples + 1) *
						  (cfg.sample_time + 1) *
						  TCS3410_MEASUREMENT_INTERVAL_US / 1000),
					  TCS3410_TIME_NORM_INTERVAL_MS));
		if (cfg.wait_time <= 0)
			return EC_RES_INVALID_PARAM;

		/* Allow deivce to go in IDLE mode */
		device.pon(true);

		/* Device is in SLEEP mode, prepare for IDLE */
		device.sai(AMS_SAI_DISABLE);
		device.sai(AMS_SAI_CLEAR);

		sensor_config(AMS_CONFIG_BASE, &cfg);
	}
	return ret;
}

static int tcs3410_get_data_rate(const struct motion_sensor_t *s)
{
  return 400;
// return TCS3410_DRV_DATA(s)->rate;
}

static int tcs3410_rgb_get_data_rate(const struct motion_sensor_t *s)
{
	return tcs3410_get_data_rate(s - 1);
}

static int tcs3410_rgb_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	return EC_SUCCESS;
}

static int tcs3410_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	s->current_range = range;
	return EC_SUCCESS;
}

ams_errno_t ams_device_status(void *stat)
{
    ams_errno_t ret = AMS_SUCCESS;

    if (device.status)
    {
        ret = device.status(stat);
    }
    else
    {
        ret = AMS_CLI_FAILURE;
        AMS_LOG_PRINTF(LOG_INFO, "No [%s] callback defined for this sensor.", __func__);
    }

    return(ret);
}

/* bitmap of registers that are in use */
static uint8_t reg_in_use[MAX_REGS / 8] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 0x00 - 0x3f */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 0x40 - 0x7f */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    /* 0x80 - 0xbf */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,    /* 0xc0 - 0xff */
};

size_t ams_registers_get(char *buf, int bufsiz)
{
    uint8_t regval[16];
    int i, j, k, cnt;

    /* find first non-zero bank of registers */
    for (i = 0; i < ARRAY_SIZE(reg_in_use); i++)
    {
        if (reg_in_use[i] != 0)
        {
            break;
        }
    }

    i &= ~1;  /* round down to the start of a group of 16 */
    i *= 8;   /* set to actual register id - each bit in the map represents a register byte*/

    cnt = 0;

    /* Print the index along the top of the registers */
    cnt += snprintf(buf + cnt, bufsiz - cnt, "     ");
    for (k = 0; k < 16; k++)
    {
        cnt += snprintf(buf + cnt, bufsiz - cnt, " %01x ", k);
        if (k == 7)
        {
            cnt += snprintf(buf + cnt, bufsiz - cnt, "  ");
        }
    }
    cnt += snprintf(buf + cnt, bufsiz - cnt, "\n     -------------------------------------------------\n");

    /* Dump the registers */
    for (; i < MAX_REGS; i += 16)
    {
        cnt += snprintf(buf + cnt, bufsiz - cnt, "%02x: ", i);
        ams_device_read(i, &regval[0], 16);

        for (j = 0; j < 16; j++)
        {
            if (reg_in_use[(i >> 3) + (j >> 3)] & (1 << (j & 7)))
            {
                cnt += snprintf(buf + cnt, bufsiz - cnt, " %02x", regval[j]);
            }
            else
            {
                cnt += snprintf(buf + cnt, bufsiz - cnt, " --");
            }

            if (j == 7)
            {
                cnt += snprintf(buf + cnt, bufsiz - cnt, "  ");
            }
        }
        cnt += snprintf(buf + cnt, bufsiz - cnt, "\n");
    }

    cnt += snprintf(buf + cnt, bufsiz - cnt, "\n");
    return(cnt);
}

const struct accelgyro_drv tcs3410_drv = {
	.init = tcs3410_init,
	.read = tcs3410_read,
	.set_range = tcs3410_set_range,
	.set_data_rate = tcs3410_set_data_rate,
	.get_data_rate = tcs3410_get_data_rate,
	.irq_handler = tcs3410_irq_handler,
};

const struct accelgyro_drv tcs3410_rgb_drv = {
	.init = tcs3410_rgb_init,
	.read = tcs3410_rgb_read,
	.set_range = tcs3410_rgb_set_range,
	.set_data_rate = tcs3410_rgb_set_data_rate,
	.get_data_rate = tcs3410_rgb_get_data_rate,
};

