/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * LSM6DSM (also LSM6DSL) accelerometer and gyro module for Chrome EC
 * 3D digital accelerometer & 3D digital gyroscope
 * This driver support both device LSM6DSM and LSM6DSL
 */

#include "hooks.h"
#include "math_util.h"
#include "task.h"
#include "driver/stm_mems_common.h"

struct stprivate_data lsm6dsm_a_data;
struct stprivate_data lsm6dsm_g_data;
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
struct stprivate_data lsm6dsm_m_data;
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

#ifdef CONFIG_ACCEL_FIFO
/* number of data samples in FIFO pattern */
static int total_samples_in_pattern;
/* LSM6DSx hw FIFO is shared between Acc/Gyro/Extern sensors */
static uint8_t fifo[FIFO_READ_LEN];
#endif /* CONFIG_ACCEL_FIFO */

/**
 * @return output base register for sensor
 */
static inline int get_xyz_reg(enum motionsensor_type type)
{
	return LSM6DSM_ACCEL_OUT_X_L_ADDR -
		(LSM6DSM_ACCEL_OUT_X_L_ADDR - LSM6DSM_GYRO_OUT_X_L_ADDR) * type;
}

#ifdef CONFIG_ACCEL_INTERRUPTS

#ifdef CONFIG_ACCEL_FIFO_THRES
static int config_threshold(const struct motion_sensor_t *s, uint16_t thr)
{
	int ret;

	if (thr > CONFIG_ACCEL_FIFO_THRES) {
		return EC_ERROR_INVAL;
	}

	/* configure FIFO watermark level. Threshold is 11 bit field */
	ret = raw_write8(s->port, s->addr, LSM6DSM_FIFO_CTRL1_ADDR,
			 thr & LSM6DSM_FIFO_WMASK_L);
	if (ret != EC_SUCCESS)
		return ret;

	ret = raw_write8(s->port, s->addr, LSM6DSM_FIFO_CTRL2_ADDR,
			 (thr >> 8 & LSM6DSM_FIFO_WMASK_H));
	return ret;
}
#endif /* CONFIG_ACCEL_FIFO */

/**
 * Configure interrupt int 1 to fire handler for:
 *
 * FIFO threshold on watermark
 * Significant Motion
 * Tap
 *
 * @s: Motion sensor pointer
 */
static int config_interrupt(const struct motion_sensor_t *s)
{
	int ret = EC_SUCCESS;

#ifdef CONFIG_GESTURE_SIGMO
	/* configure significant motion as 7 step */
	ret = st_write_data_with_mask(s, LSM6DSM_CTRL10_ADDR,
				      LSM6DSM_FUNC_EN_MASK,
				      LSM6DSM_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;

	ret = st_write_data_with_mask(s, LSM6DSM_CTRL10_ADDR,
				      LSM6DSM_SIG_MOT_MASK,
				      LSM6DSM_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;

	/* enable interrupt on sig. motion and route to int1 */
	ret = st_write_data_with_mask(s, LSM6DSM_FIFO_INT1_CTRL,
				      LSM6DSM_INT1_SIGN_MASK,
				      LSM6DSM_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;
#endif

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	/* enable interrupt on tap and route on int1 */
	ret = raw_write8(s->port, s->addr, LSM6DSM_LIR_ADDR,
			 LSM6DSM_EN_INT | LSM6DSM_EN_TAP);
	if (ret != EC_SUCCESS)
		return ret;

	/* configure tap duration */
	ret = st_write_data_with_mask(s, LSM6DSM_TAP_THS_6D,
			LSM6DSM_D4D_EN_MASK | LSM6DSM_TAP_TH_MASK, 0x89);
	if (ret != EC_SUCCESS)
		return ret;

	ret = raw_write8(s->port, s->addr, LSM6DSM_INT_DUR2_ADDR, 0x06);
	if (ret != EC_SUCCESS)
		return ret;

	ret = raw_write8(s->port, s->addr, LSM6DSM_WUP_THS_ADDR, 0x00);
	if (ret != EC_SUCCESS)
		return ret;

	ret = st_write_data_with_mask(s, LSM6DSM_MD1_CFG_ADDR,
				      LSM6DSM_INT1_STAP_MASK,
				      LSM6DSM_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */

#ifdef CONFIG_ACCEL_FIFO_THRES
	ret = config_threshold(s, CONFIG_ACCEL_FIFO_THRES);
	if (ret != EC_SUCCESS)
		return ret;

	/* enable interrupt on FIFO watermask and route to int1 */
	ret = st_write_data_with_mask(s, LSM6DSM_FIFO_INT1_CTRL,
				      LSM6DSM_FTH_INT1_MASK, LSM6DSM_EN_BIT);
#endif /* CONFIG_ACCEL_FIFO */

	return ret;
}

/**
 * lsm6dsm_interrupt - interrupt from int1/2 pin of sensor
 */
void lsm6dsm_interrupt(enum gpio_signal signal)
{
	task_set_event(TASK_ID_MOTIONSENSE,
		       CONFIG_ACCEL_LSM6DSM_INT_EVENT, 0);
}

/**
 * irq_handler - bottom half of the interrupt stack
 */
static int irq_handler(struct motion_sensor_t *s, uint32_t *event)
{
	int ret = EC_SUCCESS;

	if ((s->type != MOTIONSENSE_TYPE_ACCEL) ||
	    (!(*event & CONFIG_ACCEL_LSM6DSM_INT_EVENT))) {
		return EC_ERROR_NOT_HANDLED;
	}

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	{
		int tmp;

		/* read int source register */
		ret = raw_read8(s->port, s->addr, LSM6DSM_TAP_SRC_ADDR, &tmp);
		if (ret != EC_SUCCESS)
			return ret;

		if (tmp & LSM6DSM_STAP_DETECT)
			*event |= CONFIG_GESTURE_TAP_EVENT;
	}
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */
#ifdef CONFIG_GESTURE_SIGMO
	{
		int tmp;

		/* read int source 1 register */
		ret = raw_read8(s->port, s->addr, LSM6DSM_FUNC_SRC1_ADDR, &tmp);
		if (ret != EC_SUCCESS)
			return ret;

		if (tmp & LSM6DSM_SIGN_MOTION_IA)
			*event |= CONFIG_GESTURE_SIGMO_EVENT;
	}
#endif /* CONFIG_GESTURE_SIGMO */

	return ret;
}
#endif /* CONFIG_ACCEL_INTERRUPTS */

#ifdef CONFIG_GESTURE_HOST_DETECTION
/*
 * Manage gesture recognition
 */
int manage_activity(const struct motion_sensor_t *s,
		    enum motionsensor_activity activity, int enable,
		    const struct ec_motion_sense_activity *param)
{
	int ret;
	struct stprivate_data *drv_data = s->drv_data;

	switch (activity) {
#ifdef CONFIG_GESTURE_SIGMO
	case MOTIONSENSE_ACTIVITY_SIG_MOTION:
		/* enable/disable interrupt on sig. motion and route to int1 */
		ret = st_write_data_with_mask(s, LSM6DSM_FIFO_INT1_CTRL,
				LSM6DSM_INT1_SIGN_MASK,
				(enable ? LSM6DSM_EN_BIT : LSM6DSM_DIS_BIT));
		if (ret != EC_SUCCESS)
			return ret;
		break;
#endif /* CONFIG_GESTURE_SIGMO */

#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	case MOTIONSENSE_ACTIVITY_DOUBLE_TAP:
		/* enable/disable interrupt on tap detection and route to int1 */
		ret = st_write_data_with_mask(s, LSM6DSM_MD1_CFG_ADDR,
				LSM6DSM_INT1_STAP_MASK,
				(enable ? LSM6DSM_EN_BIT : LSM6DSM_DIS_BIT));
		if (ret != EC_SUCCESS)
			return ret;
		break;
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */
	default:
		/* unhandled activity */
		ret = EC_RES_INVALID_PARAM;
		break;
	}

	if (ret == EC_SUCCESS) {
		if (enable) {
			drv_data->en_activities |= 1 << activity;
			drv_data->dis_activities &= ~(1 << activity);
		} else {
			drv_data->en_activities &= ~(1 << activity);
			drv_data->dis_activities |= 1 << activity;
		}
	}

	return ret;
}

int list_activities(const struct motion_sensor_t *s, uint32_t *enabled,
		    uint32_t *disabled)
{
	struct stprivate_data *drv_data = s->drv_data;

	*enabled = drv_data->en_activities;
	*disabled = drv_data->dis_activities;

	return EC_RES_SUCCESS;
}
#endif /* CONFIG_GESTURE_HOST_DETECTION */

#ifdef CONFIG_ACCEL_FIFO
/**
 * fifo_enable - enable/disable fifo
 * @s: Motion sensor pointer
 * @status: 0 disable, 1 enable
 */
static int fifo_enable(const struct motion_sensor_t *s, int status)
{
	uint8_t reg_value;

	if (status)
		reg_value = LSM6DSM_FIFO_ODR_MAX_VAL;
	else
		reg_value = LSM6DSM_FIFO_ODR_OFF_VAL;

	return st_write_data_with_mask(s, LSM6DSM_FIFO_CTRL5_ADDR,
				       LSM6DSM_FIFO_CTRL5_ODR_MASK,
				       reg_value);
}

/**
 * set_fifo_mode - set fifo mode
 * @s: Motion sensor pointer
 * @fmode: BYPASS or CONTINUOS
 */
static int set_fifo_mode(const struct motion_sensor_t *s, enum fifo_mode fmode)
{
	int err, enable_fifo;
	uint8_t reg_value;

	switch (fmode) {
	case BYPASS:
		reg_value = LSM6DSM_FIFO_MODE_BYPASS_VAL;
		enable_fifo = 0;
		break;
	case CONTINUOS:
		reg_value = LSM6DSM_FIFO_MODE_CONTINUOS_VAL;
		enable_fifo = 1;
		break;
	default:
		return EC_ERROR_INVAL;
	}

	err = fifo_enable(s, enable_fifo);
	if (err != EC_SUCCESS)
		return err;

	return st_write_data_with_mask(s, LSM6DSM_FIFO_CTRL5_ADDR,
				       LSM6DSM_FIFO_CTRL5_MODE_MASK,
				       reg_value);
}

/**
 * set_fifo_params - Configure internal FIFO parameters
 *
 * Configure FIFO decimator to have every time the right pattern
 * with acc/gyro
 */
static int set_fifo_params(struct motion_sensor_t *s)
{
	int err;
	uint8_t decimator, decimator_mask;
	unsigned int min_odr = LSM6DSM_ODR_MAX_VAL, max_odr = 0;
	uint16_t fifo_len = LSM6DSM_MAX_FIFO_SIZE;
	uint16_t min_num_pattern, j;
	uint16_t max_num_pattern;
	struct stprivate_data *drvdata;

	/* search for min and max odr values for acc, gyro and mag */
	for (j = FIFO_DEV_GYRO; j < FIFO_DEV_NUM; j++) {
		drvdata = (s + j)->drv_data;

		/* check if sensor enabled with ODR */
		if (drvdata->base.odr > LSM6DSM_ODR_0HZ_VAL) {
			if (min_odr > drvdata->base.odr)
				min_odr = drvdata->base.odr;
			if (max_odr < drvdata->base.odr)
				max_odr = drvdata->base.odr;
		}
	}

	/* disable FIFO */
	if (max_odr == 0)
		return 0;

	/* scan all sensors configuration to calculate FIFO decimator */
	total_samples_in_pattern = 0;
	for (j = FIFO_DEV_GYRO, min_num_pattern = 0; j < FIFO_DEV_NUM; j++) {
		drvdata = (s + j)->drv_data;
		if (drvdata->base.odr > LSM6DSM_ODR_0HZ_VAL) {
			drvdata->samples_in_pattern =
				drvdata->base.odr / min_odr;
			drvdata->num_pattern =
				MAX(fifo_len / drvdata->samples_in_pattern, 1);
			decimator = LSM6DSM_FIFO_DECIMATOR(
						max_odr / drvdata->base.odr);
		} else {
			/* not in FIFO if sensor disabled */
			drvdata->samples_in_pattern = 0;
			decimator = 0;
		}

		/* set FIFO decimator for each sensor */
		switch((s + j)->type) {
		case MOTIONSENSE_TYPE_ACCEL:
			decimator_mask = LSM6DSM_FIFO_CTRL3_DEC_XL_MASK;
			err = st_write_data_with_mask(s,
						      LSM6DSM_FIFO_CTRL3_ADDR,
						      decimator_mask,
						      decimator);
			if (err != EC_SUCCESS)
				return err;
			break;
		case MOTIONSENSE_TYPE_GYRO:
			decimator_mask = LSM6DSM_FIFO_CTRL3_DEC_G_MASK;
			err = st_write_data_with_mask(s,
						      LSM6DSM_FIFO_CTRL3_ADDR,
						      decimator_mask,
						      decimator);
			if (err != EC_SUCCESS)
				return err;
			break;
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
		/* when mag in FIFO trigger is fired by acc */
		case MOTIONSENSE_TYPE_MAG:
			drvdata = (s + j)->drv_data;
			decimator_mask = LSM6DSM_FIFO_CTRL4_DEC_M_MASK;
			err = st_write_data_with_mask(s,
						      LSM6DSM_FIFO_CTRL4_ADDR,
						      decimator_mask,
						      decimator);
			if (err != EC_SUCCESS)
				return err;
			break;
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */
		default:
			return EC_ERROR_INVAL;
		}

		min_num_pattern =
			MIN_AZ(min_num_pattern, drvdata->num_pattern);
		total_samples_in_pattern += drvdata->samples_in_pattern;
	}

	/* calculate MAX pattern number in FIFO */
	if (total_samples_in_pattern > 0) {
		max_num_pattern = LSM6DSM_MAX_FIFO_SIZE /
				(total_samples_in_pattern * OUT_XYZ_SIZE);
		if (min_num_pattern > max_num_pattern)
			min_num_pattern = max_num_pattern;
	}

	fifo_len = total_samples_in_pattern * min_num_pattern * OUT_XYZ_SIZE;

	return fifo_len;
}

/*
 * Must order FIFO read based on ODR:
 * Fox examples Acc @ 52 Hz, Gyro @ 26 Hz Mag @ 13 Hz in FIFO we have
 * for each pattern this data samples:
 *  ________ _______ _______ _______ ________ _______ _______
 * | Gyro_0 | Acc_0 | Mag_0 | Acc_1 | Gyro_1 | Acc_2 | Acc_3 |
 * |________|_______|_______|_______|________|_______|_______|
 *
 * Total samples for each pattern: 2 Gyro, 4 Acc, 1 Mag
 */
static int fifo_order(int id, uint8_t *samples_in_pattern, uint8_t *ratio)
{
	int j;
	int ret = 1;
	int r_jid, r_idj;
	int tot = 0;

	/* no more samples for this sensor */
	if (samples_in_pattern[id] == 0)
		return 1;

	for (j = FIFO_DEV_GYRO; j < FIFO_DEV_NUM; j++) {
		if (j == id)
			continue;
		tot += samples_in_pattern[j];
	}

	if (tot == 0)
		return 0;

	for (j = FIFO_DEV_GYRO; j < FIFO_DEV_NUM; j++) {
		/* skip current sensor */
		if (j == id)
			continue;

		if (samples_in_pattern[j] == 0)
			continue;

		r_jid = ratio[j] * samples_in_pattern[id];
		r_idj = ratio[id] * samples_in_pattern[j];

		if (r_jid >= r_idj)
			ret = 0;
	}

	return ret;
}

/**
 * push_fifo_data - Scan data pattern and push upside
 */
static void push_fifo_data(struct motion_sensor_t *s, uint16_t flen)
{
	int i, j;
	uint8_t fifo_offset = 0;
	uint8_t total_samples;
	uint8_t samples_in_pattern[FIFO_DEV_NUM];
	uint8_t ratio[FIFO_DEV_NUM];
	int *axis;
	/* in FIFO sensors are mapped in a different way */
	uint8_t agm_maps[] = {
		BASE_GYRO,
		BASE_ACCEL,
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
		BASE_MAG,
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */
		};

	while (fifo_offset < flen) {
		struct stprivate_data *drvdata;
		for (i = FIFO_DEV_GYRO, total_samples = 0;
		     i < FIFO_DEV_NUM;
		     i++) {
			/* remap index on sensor */
			j = agm_maps[i];
			drvdata = (s + j)->drv_data;
			samples_in_pattern[j] = drvdata->samples_in_pattern;
			ratio[j] = drvdata->samples_in_pattern;
		}

		total_samples = total_samples_in_pattern;
		do {
			/* gyro samples (if any) before other by design
			 * only @ first loop */
			for (j = FIFO_DEV_GYRO; j < FIFO_DEV_NUM; j++) {
				struct ec_response_motion_sensor_data vect;

				/* remap index on sensor */
				i = agm_maps[j];

				if (fifo_order(i, samples_in_pattern, ratio))
					continue;

				axis = (s + i)->raw_xyz;

				/* apply precision, sensitivity and rotation */
				st_normalize(s + i, axis, &fifo[fifo_offset]);
				vect.data[0] = axis[0];
				vect.data[1] = axis[1];
				vect.data[2] = axis[2];

				/* fill vector array */
				vect.flags = 0;
				vect.sensor_num = (s + i - motion_sensors);
				motion_sense_fifo_add_unit(&vect, s + i, 3);

				fifo_offset += OUT_XYZ_SIZE;
				samples_in_pattern[i]--;
				total_samples--;
			}
		} while (total_samples > 0);
	}
}

static int load_fifo(struct motion_sensor_t *s)
{
	int err, left;
	uint16_t byte_in_pattern;
	struct fstatus fsts;

	if (s->type != MOTIONSENSE_TYPE_ACCEL)
		return EC_SUCCESS;

	/* read how many data pattern on FIFO to read and pattern */
	err = st_raw_read_n_noinc(s->port, s->addr, LSM6DSM_FIFO_STS1_ADDR,
			 (uint8_t *)&fsts, sizeof(struct fstatus));
	if (err != EC_SUCCESS)
		return err;

	if (fsts.len & LSM6DSM_FIFO_DATA_OVR) {
		CPRINTF("[%T %s FIFO Overrun]", s->name);
		return EC_ERROR_INVAL;
	}

	/* DIFF[9:0] are number of unread uint16 in FIFO
	 * mask DIFF and compute total byte len to read from FIFO */
	fsts.len &= LSM6DSM_FIFO_DIFF_MASK;
	fsts.len *= sizeof(uint16_t);
	byte_in_pattern = total_samples_in_pattern * OUT_XYZ_SIZE;
	if (byte_in_pattern == 0)
		return EC_SUCCESS;

	/* normalize in case not multiple of byte_in_pattern */
	fsts.len = (fsts.len / byte_in_pattern) * byte_in_pattern;
	if (fsts.len == 0)
		return EC_SUCCESS;

	left = fsts.len;

	/* push all data on upper side */
	do {
		/* fit len to pre-allocated static buffer */
		fsts.len = left;
		if (fsts.len > FIFO_READ_LEN) {
			fsts.len = FIFO_READ_LEN;
			CPRINTF("Partial Read %d (%d)", fsts.len, left);
		}

		/* check pattern data in FIFO */
		if (fsts.pattern != 0) {
			int flush = byte_in_pattern - fsts.pattern;
			int burst = flush;
			/* pattern must be always 0 anyway remove this pattern
			 * from FIFO and calculate new data len: this pattern
			 * is trushed and some data is lost. Some high ODR may
			 * cause this when ODR is changed because FIFO confi-
			 * guration change to BYPASS mode at runtime
			 */
			while(flush > 0) {
				if (burst > FIFO_READ_LEN)
					burst = FIFO_READ_LEN;

				err = st_raw_read_n_noinc(s->port, s->addr,
					LSM6DSM_FIFO_DATA_ADDR, fifo, burst);
				if (err != EC_SUCCESS)
					return err;

				fsts.len -= flush;

				CPRINTF("[%T %s Flush %d (left %d)]",
					s->name, burst, fsts.len);
				if (fsts.len == 0)
					return EC_SUCCESS;

				flush -= burst;
				burst = flush;
				}
		}

		/* read data and copy in buffer */
		err = st_raw_read_n_noinc(s->port, s->addr,
					  LSM6DSM_FIFO_DATA_ADDR,
					  fifo, fsts.len);
		if (err != EC_SUCCESS)
			return err;

		/* manage patterns and push data */
		push_fifo_data(s, fsts.len);
		left -= fsts.len;
	} while(left > 0);

	return EC_SUCCESS;
}

/**
 * configure_fifo - update mode and ODR for FIFO decimator
 */
static int configure_fifo(void)
{
	int err, fifo_len;
	struct motion_sensor_t *s = motion_sensors;

	/* changing in ODR must stop FIFO */
	err = set_fifo_mode(s, BYPASS);
	if (err != EC_SUCCESS)
		return err;

	fifo_len = set_fifo_params(s);
	if (fifo_len < 0)
		return EC_ERROR_INVAL;

	if (fifo_len > 0) {
		err = set_fifo_mode(s, CONTINUOS);
		if (err != EC_SUCCESS)
			return err;
	}

	return err;
}

/* perform_calib
 * @s: Motion sensor pointer
 */
int perform_calib(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
/**
 * Configure passthrough for I2C interface:
 * @mode - PT_DISABLE disable, PT_ENABLE enable
 */
static const struct motion_sensor_t *get_master_sensor(void)
{
	int i;

	/* must use LSM6DSM/L I2C address */
	for (i = 0; i < motion_sensor_count; i++) {
		if (motion_sensors[i].chip == MOTIONSENSE_CHIP_LSM6DSM)
			break;
	}

	if (i == motion_sensor_count) {
		CPRINTF("[%T No I2C Master for passthrough]");
		return NULL;
	}

	return &motion_sensors[i];
}

/**
 * Wait sensorHub end operation on I2C master interface:
 * @s: Motion sensor pointer
 * @timeout - Max wait time (ms)
 */
static int wait_sensor_hub_op(const struct motion_sensor_t *s, int timeout)
{
	int tmo = 0, tmp, ret;

	do {
		/* wait end of operation */
		ret = raw_read8(s->port, s->addr, LSM6DSM_FUNC_SRC1, &tmp);
		if (ret != EC_SUCCESS)
			return ret;
		if (tmp & LSM6DSM_SENSORHUB_END_OP)
			return EC_SUCCESS;
		msleep(5);
		tmo += 5;
	} while (tmo < timeout);

	return EC_ERROR_TIMEOUT;
}

/**
 * Configure lsm6dsm/l sensor hub to work with mag in FIFO
 * @s: Motion sensor pointer
 *
 * Select max mag odr than use acc trigger and FIFO decimator to obtain
 * right data rate
 */
static int init_lis2mdl_fifo(const struct motion_sensor_t *s)
{
	int ret, tmp;
	const struct motion_sensor_t *ps;

	/* search I2C master device */
	ps = get_master_sensor();

	if (!ps)
		return EC_ERROR_OVERFLOW;

	mutex_lock(s->mutex);

	/* save ODR for Acc */
	ret = raw_read8(ps->port, ps->addr, LSM6DSM_CTRL1_ADDR, &tmp);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	/* configure Mag Cont. Mode and ODR 100 Hz */
	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL1_ADDR, 0x00);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_FUNC_CFG_ACCESS,
			 LSM6DSM_FUNC_ENABLE_MASK);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV0_ADD, s->addr);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV0_SUBADD,
			 LIS2MDL_CFG_REG_A);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_DATA_WRITE_SUB_SLV0,
			 LIS2MDL_ODR100_HZ | LIS2MDL_CONT_MODE);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV0_CONFIG, 0x10);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV1_CONFIG,
			 LSM6DSM_SLVCFG_WONCE_BIT);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_FUNC_CFG_ACCESS, 0);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL10_ADDR,
			 LSM6DSM_FUNC_EN_MASK);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_MASTER_CONFIG,
			 LSM6DSM_PULLUP_EN | LSM6DSM_MASTER_ENABLE);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	/* trigger for write configuration data in mag register */
	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL1_ADDR, 0x80);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = wait_sensor_hub_op(ps, 50);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	/* configure sensor hub FIFO data read address and len */
	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL10_ADDR, 0);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_MASTER_CONFIG, 0);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL1_ADDR, 0);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_FUNC_CFG_ACCESS,
			 LSM6DSM_FUNC_ENABLE_MASK);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV0_ADD, s->addr | 1);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV0_SUBADD, LIS2MDL_OUT_REG);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_SLV0_CONFIG, OUT_XYZ_SIZE);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_FUNC_CFG_ACCESS, 0);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL10_ADDR,
			 LSM6DSM_FUNC_EN_MASK);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_MASTER_CONFIG,
			 LSM6DSM_PULLUP_EN | LSM6DSM_MASTER_ENABLE);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL1_ADDR, 0x80);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	ret = wait_sensor_hub_op(ps, 50);
	if (ret != EC_SUCCESS)
		goto unlock_mutex;

	/* restore ODR in Acc */
	ret = raw_write8(ps->port, ps->addr, LSM6DSM_CTRL1_ADDR, tmp);

unlock_mutex:
	mutex_unlock(s->mutex);
	ret = configure_fifo();

	return ret;
}

#ifdef CONFIG_LSM6DSM_PASSTHROUGH
static int set_passthrough(int mode)
{
	int ret, i;
	struct motion_sensor_t *s = motion_sensors;

	/* must use LSM6DSM/L I2C address */
	for (i = 0; i < motion_sensor_count; i++) {
		if (s->chip == MOTIONSENSE_CHIP_LSM6DSM)
			break;
		s++;
	}

	if (i == motion_sensor_count) {
		CPRINTF("[%T No I2C Master for passthrough]");
		return EC_ERROR_OVERFLOW;
	}

	ret = st_write_data_with_mask(s, LSM6DSM_CTRL10_ADDR,
				      LSM6DSM_FUNC_EN_MASK, LSM6DSM_EN_BIT);
	if (ret != EC_SUCCESS)
		return ret;

	if (mode == PT_ENABLE)
		ret = raw_write8(s->port, s->addr, LSM6DSM_MASTER_CONFIG,
				 LSM6DSM_PULLUP_EN | LSM6DSM_PASSTROUGH_MODE);
	else
		ret = st_write_data_with_mask(s, LSM6DSM_MASTER_CONFIG,
					      LSM6DSM_FUNC_EN_MASK, 0);

	return ret;
}
#endif /* CONFIG_LSM6DSM_PASSTHROUGH */
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */
#endif /* CONFIG_ACCEL_FIFO */

/**
 * set_range - set full scale range
 * @s: Motion sensor pointer
 * @range: Range
 * @rnd: Round up/down flag
 * Note: Range is sensitivity/gain for speed purpose
 */
static int set_range(const struct motion_sensor_t *s, int range, int rnd)
{
	int err;
	uint8_t ctrl_reg, reg_val;
	struct stprivate_data *data = s->drv_data;
	int newrange = range;

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
	if (s->type == MOTIONSENSE_TYPE_MAG) {
		/* formmag range is fixed */
		data->base.range = LIS2MDL_SENSITIVITY;
		return EC_SUCCESS;
		}
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

	ctrl_reg = LSM6DSM_RANGE_REG(s->type);
	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		/* adjust and check rounded value for acc */
		if (rnd && (newrange < LSM6DSM_ACCEL_NORMALIZE_FS(newrange)))
			newrange <<= 1;

		if (newrange > LSM6DSM_ACCEL_FS_MAX_VAL)
			newrange = LSM6DSM_ACCEL_FS_MAX_VAL;

		reg_val = LSM6DSM_ACCEL_FS_REG(newrange);
	} else {
		/* adjust and check rounded value for gyro */
		if (rnd && (newrange < LSM6DSM_GYRO_NORMALIZE_FS(newrange)))
			newrange <<= 1;

		if (newrange > LSM6DSM_GYRO_FS_MAX_VAL)
			newrange = LSM6DSM_GYRO_FS_MAX_VAL;

		reg_val = LSM6DSM_GYRO_FS_REG(newrange);
		}

	mutex_lock(s->mutex);
	err = st_write_data_with_mask(s, ctrl_reg, LSM6DSM_RANGE_MASK, reg_val);
	if (err == EC_SUCCESS)
		/* save internally gain for speed optimization */
		data->base.range = (s->type == MOTIONSENSE_TYPE_ACCEL ?
				    LSM6DSM_ACCEL_FS_GAIN(newrange) :
				    LSM6DSM_GYRO_FS_GAIN(newrange));
	mutex_unlock(s->mutex);

	return EC_SUCCESS;
}

/**
 * get_range - get full scale range
 * @s: Motion sensor pointer
 *
 * For mag range is fixed to LIS2MDL_RANGE by hardware
 */
static int get_range(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	if (MOTIONSENSE_TYPE_ACCEL == s->type)
		return LSM6DSM_ACCEL_GAIN_FS(data->base.range);
#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
	if (s->type == MOTIONSENSE_TYPE_MAG)
		/* for mag range is fixed */
		return LIS2MDL_RANGE;
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

	return LSM6DSM_GYRO_GAIN_FS(data->base.range);
}

/**
 * set_data_rate
 * @s: Motion sensor pointer
 * @range: Rate (mHz)
 * @rnd: Round up/down flag
 *
 * For mag in cascade with lsm6dsm/l we use acc trigger and FIFO decimator
 */
static int set_data_rate(const struct motion_sensor_t *s, int rate, int rnd)
{
	int ret, normalized_rate = LSM6DSM_ODR_MIN_VAL;
	struct stprivate_data *data = s->drv_data;
	uint8_t ctrl_reg, reg_val;

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
	if (MOTIONSENSE_TYPE_MAG == s->type) {
		if (rate == LSM6DSM_ODR_0HZ_VAL) {
			/* power off mag (only software) */
			data->base.odr = LSM6DSM_ODR_0HZ_VAL;
		} else {
			normalized_rate = LSM6DSM_ODR_TO_NORMALIZE(rate);
		}

		if (rnd && (normalized_rate < rate))
			normalized_rate <<= 1;

		/* adjust rounded value for acc and gyro because ODR are shared */
		if (normalized_rate > LSM6DSM_ODR_MAX_VAL) {
			normalized_rate = LSM6DSM_ODR_MAX_VAL;
		} else if (normalized_rate < LSM6DSM_ODR_MIN_VAL) {
			normalized_rate = LSM6DSM_ODR_MIN_VAL;
		}

		data->base.odr = normalized_rate;

#ifdef CONFIG_ACCEL_FIFO
		configure_fifo();
#endif /* CONFIG_ACCEL_FIFO */

		return EC_SUCCESS;
	}
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

	ctrl_reg = LSM6DSM_ODR_REG(s->type);

	if (rate == LSM6DSM_ODR_0HZ_VAL) {
		/* power off acc/gyro */
		mutex_lock(s->mutex);

		ret = st_write_data_with_mask(s, ctrl_reg, LSM6DSM_ODR_MASK,
					      LSM6DSM_ODR_0HZ_VAL);
		if (ret == EC_SUCCESS) {
			data->base.odr = LSM6DSM_ODR_0HZ_VAL;

		mutex_unlock(s->mutex);

#ifdef CONFIG_ACCEL_FIFO
			configure_fifo();
#endif /* CONFIG_ACCEL_FIFO */
		}

		return ret;
	}

	reg_val = LSM6DSM_ODR_TO_REG(rate);
	normalized_rate = LSM6DSM_ODR_TO_NORMALIZE(rate);

	if (rnd && (normalized_rate < rate)) {
		reg_val++;
		normalized_rate <<= 1;
	}

	/* adjust rounded value for acc and gyro because ODR are shared */
	if (reg_val > LSM6DSM_ODR_416HZ_VAL) {
		reg_val = LSM6DSM_ODR_416HZ_VAL;
		normalized_rate = LSM6DSM_ODR_MAX_VAL;
	} else if (reg_val < LSM6DSM_ODR_13HZ_VAL) {
		reg_val = LSM6DSM_ODR_13HZ_VAL;
		normalized_rate = LSM6DSM_ODR_MIN_VAL;
	}

	mutex_lock(s->mutex);
	ret = st_write_data_with_mask(s, ctrl_reg, LSM6DSM_ODR_MASK, reg_val);
	if (ret == EC_SUCCESS)
		data->base.odr = normalized_rate;

	mutex_unlock(s->mutex);

#ifdef CONFIG_ACCEL_FIFO
	configure_fifo();
#endif /* CONFIG_ACCEL_FIFO */

	return ret;
}

static int is_data_ready(const struct motion_sensor_t *s, int *ready)
{
	int ret, tmp;

	ret = raw_read8(s->port, s->addr, LSM6DSM_STATUS_REG, &tmp);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X RS Error]", s->name, s->type);
		return ret;
	}

	if (MOTIONSENSE_TYPE_ACCEL == s->type)
		*ready = (LSM6DSM_STS_XLDA_UP == (tmp & LSM6DSM_STS_XLDA_MASK));
	else
		*ready = (LSM6DSM_STS_GDA_UP == (tmp & LSM6DSM_STS_GDA_MASK));

	return EC_SUCCESS;
}

/*
 * Is not very efficient to collect the data in read: better have an interrupt
 * and collect the FIFO, even if it has one item: we don't have to check if the
 * sensor is ready (minimize I2C access)
 */
static int read(const struct motion_sensor_t *s, vector_3_t v)
{
	uint8_t raw[OUT_XYZ_SIZE];
	uint8_t xyz_reg;
	int ret, i, range, tmp = 0;
	struct stprivate_data *data = s->drv_data;

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
	/* mag doesn't support read in fifo mode */
	if (s->type == MOTIONSENSE_TYPE_MAG)
		return EC_ERROR_UNIMPLEMENTED;
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

	ret = is_data_ready(s, &tmp);
	if (ret != EC_SUCCESS)
		return ret;

	/*
	 * If sensor data is not ready, return the previous read data.
	 * Note: return success so that motion senor task can read again
	 * to get the latest updated sensor data quickly.
	 */
	if (!tmp) {
		if (v != s->raw_xyz)
			memcpy(v, s->raw_xyz, sizeof(s->raw_xyz));
		return EC_SUCCESS;
	}

	xyz_reg = get_xyz_reg(s->type);

	/* read data bytes starting at xyz_reg */
	ret = st_raw_read_n_noinc(s->port, s->addr, xyz_reg, raw, OUT_XYZ_SIZE);
	if (ret != EC_SUCCESS)
		return ret;

	/* apply precision, sensitivity and rotation vector */
	st_normalize(s, v, raw);

	/* apply offset in the device coordinates */
	range = get_range(s);
	for (i = X; i <= Z; i++)
		v[i] += (data->offset[i] << 5) / range;

	return EC_SUCCESS;
}

#ifdef CONFIG_GESTURE_HOST_DETECTION
/*
 * init_activities
 * Works on Accelerometer sensor only
 * Init configured activities (Significant Motion, Tap)
 */
static void init_activities(const struct motion_sensor_t *s)
{
	struct stprivate_data *data = s->drv_data;

	data->en_activities = data->dis_activities = 0;

#ifdef CONFIG_GESTURE_SIGMO
	data->dis_activities |= (1 << MOTIONSENSE_ACTIVITY_SIG_MOTION);
#endif /* CONFIG_GESTURE_SIGMO */
#ifdef CONFIG_GESTURE_SENSOR_BATTERY_TAP
	data->dis_activities |= (1 << MOTIONSENSE_ACTIVITY_DOUBLE_TAP);
#endif /* CONFIG_GESTURE_SENSOR_BATTERY_TAP */
}
#endif /* CONFIG_GESTURE_HOST_DETECTION */

static int init(const struct motion_sensor_t *s)
{
	int ret = 0, tmp;
	struct stprivate_data *data = s->drv_data;

	/*
	 * This sensor can be powered through an EC reboot, so the state of the
	 * sensor is unknown here so reset it
	 * LSM6DSM/L supports both accel & gyro features
	 * Board will see two virtual sensor devices: accel & gyro
	 * Requirement: Accel need be init before gyro and mag
	 */
	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		ret = raw_read8(s->port, s->addr, LSM6DSM_WHO_AM_I_REG, &tmp);
		if (ret != EC_SUCCESS)
			return EC_ERROR_UNKNOWN;

		if (tmp != LSM6DSM_WHO_AM_I)
			return EC_ERROR_ACCESS_DENIED;

		mutex_lock(s->mutex);

		/* software reset */
		ret = st_write_data_with_mask(s, LSM6DSM_RESET_ADDR,
					      LSM6DSM_RESET_MASK,
					      LSM6DSM_EN_BIT);
		if (ret != EC_SUCCESS)
			goto err_unlock;

		/* output data not updated until have been read */
		ret = st_write_data_with_mask(s, LSM6DSM_BDU_ADDR,
					      LSM6DSM_BDU_MASK,
					      LSM6DSM_EN_BIT);
		if (ret != EC_SUCCESS)
			goto err_unlock;

#ifdef CONFIG_GESTURE_HOST_DETECTION
		init_activities(s);
#endif /* CONFIG_GESTURE_HOST_DETECTION */

#ifdef CONFIG_ACCEL_FIFO
		ret = set_fifo_mode(s, BYPASS);
		if (ret != EC_SUCCESS)
			goto err_unlock;
#endif /* CONFIG_ACCEL_FIFO */

#ifdef CONFIG_ACCEL_INTERRUPTS
		ret = config_interrupt(s);
		if (ret != EC_SUCCESS)
			goto err_unlock;
#endif

		mutex_unlock(s->mutex);
	}

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL

#ifdef CONFIG_LSM6DSM_PASSTHROUGH
	/* passthrough on I2C master to configure mag */
	ret = set_passthrough(PT_ENABLE);
	if (ret != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	ret = init_lis2mdl(s);
	if (ret != EC_SUCCESS) {
		CPRINTF("[%T %s type:0x%X Init Error]",
			s->name, s->type);
		set_passthrough(PT_DISABLE);
		return EC_ERROR_UNKNOWN;
	}

	ret = set_passthrough(PT_DISABLE);
	if (ret != EC_SUCCESS) {
		return EC_ERROR_UNKNOWN;
#else /* CONFIG_LSM6DSM_PASSTHROUGH */
		if (s->type == MOTIONSENSE_TYPE_MAG) {
			/* configure mag without passthrough */
			ret = init_lis2mdl_fifo(s);
#endif /* CONFIG_LSM6DSM_PASSTHROUGH */
	}
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

	ret = set_range(s, s->default_range, 1);

	/* set default resolution common to acc and gyro */
	data->resol = LSM6DSM_RESOLUTION;

	CPRINTF("[%T %s: MS Done Init type:0x%X range:%d]",
		s->name, s->type, get_range(s));

	return ret;

err_unlock:
	mutex_unlock(s->mutex);

	return EC_ERROR_UNKNOWN;
}

const struct accelgyro_drv lsm6dsm_drv = {
	.init = init,
	.read = read,
	.set_range = set_range,
	.get_range = get_range,
	.set_resolution = st_set_resolution,
	.get_resolution = st_get_resolution,
	.set_data_rate = set_data_rate,
	.get_data_rate = st_get_data_rate,
	.set_offset = st_set_offset,
	.get_offset = st_get_offset,
	.perform_calib = perform_calib,
#ifdef CONFIG_ACCEL_FIFO
	.load_fifo = load_fifo,
#endif /* CONFIG_ACCEL_FIFO */

#ifdef CONFIG_ACCEL_INTERRUPTS
	.irq_handler = irq_handler,
#endif /* CONFIG_ACCEL_INTERRUPTS */

#ifdef CONFIG_GESTURE_HOST_DETECTION
	.manage_activity = manage_activity,
	.list_activities = list_activities,
#endif
};
