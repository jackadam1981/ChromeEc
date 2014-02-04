/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelerometer.h"
#include "console.h"
#include "hooks.h"
#include "math.h"
#include "timer.h"
#include "task.h"
#include "util.h"

/* For cosine lookup table, define the increment and the size of the table. */
#define COSINE_LUT_INCR_DEG	5
#define COSINE_LUT_SIZE		((180 / COSINE_LUT_INCR_DEG) + 1)

/* Lookup table for the value of cosine from 0 degrees to 180 degrees. */
static const float cos_lut[] = {
	 1.00000,  0.99619,  0.98481,  0.96593,  0.93969,
	 0.90631,  0.86603,  0.81915,  0.76604,  0.70711,
	 0.64279,  0.57358,  0.50000,  0.42262,  0.34202,
	 0.25882,  0.17365,  0.08716,  0.00000, -0.08716,
	-0.17365, -0.25882, -0.34202, -0.42262, -0.50000,
	-0.57358, -0.64279, -0.70711, -0.76604, -0.81915,
	-0.86603, -0.90631, -0.93969, -0.96593, -0.98481,
	-0.99619, -1.00000,
};
BUILD_ASSERT(ARRAY_SIZE(cos_lut) == COSINE_LUT_SIZE);

/* Some useful math functions. */
#define SQ(x) ((x) * (x))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

/* Current acceleration vectors and current lid angle. */
static struct vector acc_lid_raw, acc_lid, acc_base;
static float lid_angle_deg;

#ifdef CONFIG_ACCEL_CALIBRATE

/*
 * Threshold to capture a sample when performing auto-calibrate. The units are
 * the same as the units of the accelerometer acceleration values.
 */
#define AUTO_CAL_DIR_THRESHOLD (ACCEL_G * 3 / 4)
#define AUTO_CAL_MAG_THRESHOLD (ACCEL_G / 25)

/* Vectors for use in recording accelerometer data for use in calibration. */
static struct vector rec_base1, rec_base2, rec_base3;
static struct vector rec_lid1, rec_lid2, rec_lid3;

#endif /* CONFIG_ACCEL_CALIBRATE */

/* Sampling interval for measuring acceleration and calculating lid angle. */
static int accel_interval_ms = 250;

#ifdef CONFIG_CMD_LID_ANGLE
static int accel_disp;
#endif

/**
 * Find acos(x) in degrees. Return 0 if x is out of range.
 */
static float arc_cos(float x)
{
	int i;

	/* Cap x if out of range. */
	if (x < -1.0)
		x = -1.0;
	else if (x > 1.0)
		x = 1.0;

	/*
	 * Increment through lookup table to find index and then linearly
	 * interpolate for precision.
	 */
	for (i = 0; i < COSINE_LUT_SIZE-1; i++)
		if (x >= cos_lut[i+1])
			return COSINE_LUT_INCR_DEG *
			(i + (cos_lut[i] - x) / (cos_lut[i] - cos_lut[i+1]));

	/* Shouldn't be possible to get here, but need to return something. */
	return 0;
}

/**
 * Take two 3 dimensional vectors and return the cosine of the angle between
 * them.
 */
static float cosine_of_angle_diff(const struct vector v1,
					const struct vector v2)
{
	int dotproduct;
	float mag1, mag2;

	/*
	 * Angle between two vectors is acos(A dot B / |A|*|B|). To return
	 * cosine of angle between vectors, then don't do acos operation.
	 */

	dotproduct = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;

	mag1 = sqrtf((float)(SQ(v1.x) + SQ(v1.y) + SQ(v1.z)));
	mag2 = sqrtf((float)(SQ(v2.x) + SQ(v2.y) + SQ(v2.z)));

	/* Check for divide by 0 although extremely unlikely. */
	if (ABS(mag1*mag2) < 0.01F)
		return 0.0;

	return (float)dotproduct / (mag1*mag2);
}

/**
 * Rotate a vector v by the 3x3 matrix R.
 */
static struct vector rotate(const struct vector v,
#ifndef CONFIG_ACCEL_CALIBRATE
			    const
#endif
			    float (* const R)[3][3])
{
	struct vector result;

	result.x = v.x * (*R)[0][0] +
		   v.y * (*R)[1][0] +
		   v.z * (*R)[2][0];
	result.y = v.x * (*R)[0][1] +
		   v.y * (*R)[1][1] +
		   v.z * (*R)[2][1];
	result.z = v.x * (*R)[0][2] +
		   v.y * (*R)[1][2] +
		   v.z * (*R)[2][2];

	return result;
}

/**
 * Calculate the lid angle using two acceleration vectors, one recorded in
 * the base and one in the lid.
 */
static float calculate_lid_angle(struct vector base, struct vector lid)
{
	struct vector up;
	float ang_lid_to_base, ang_lid_to_up;
	float lid_to_base, base_to_hinge;

	/* Rotate base to get vector pointing up. */
	up = rotate(base, &rot_up_direction);

	/*
	 * The angle between lid and base is:
	 * acos((cad(base, lid) - cad(base, hinge)^2) /(1 - cad(base, hinge)^2))
	 * where cad() is the cosine_of_angle_diff() function.
	 *
	 * Make sure to check for divide by 0.
	 */
	lid_to_base = cosine_of_angle_diff(base, lid);
	base_to_hinge = cosine_of_angle_diff(base, hinge_axis);
	base_to_hinge = SQ(base_to_hinge);

	/* Check divide by 0. */
	if (ABS(1.0F - base_to_hinge) < 0.01F)
		return 0.0;

	ang_lid_to_base = arc_cos(
			(lid_to_base - base_to_hinge) / (1 - base_to_hinge));

	/*
	 * Calculate angle between lid and up vector and use this to determine
	 * the sign of the lid angle.
	 */
	ang_lid_to_up = arc_cos(cosine_of_angle_diff(up, lid));
	if (ang_lid_to_up > 90)
		ang_lid_to_base = -ang_lid_to_base;

	return ang_lid_to_base;
}

void motion_sense_task(void)
{
	timestamp_t ts0, ts1;
	int wait_us;

	/* Initialize accelerometers. */
	accel_init(ACCEL_LID);
	accel_init(ACCEL_BASE);

	while (1) {
		ts0 = get_time();

		/* Read all accelerations. */
		accel_read(ACCEL_LID, &acc_lid_raw.x, &acc_lid_raw.y,
			   &acc_lid_raw.z);
		accel_read(ACCEL_BASE, &acc_base.x, &acc_base.y,
			   &acc_base.z);

		/*
		 * Rotate the lid vector in order to account for any mounting
		 * orientation different between the two sensors.
		 */
		acc_lid = rotate(acc_lid_raw, &rot_sense_orientation);

		/* Calculate angle of lid. */
		lid_angle_deg = calculate_lid_angle(acc_base, acc_lid);

		/*
		 * TODO: add a filter on lid angle to smooth out jumps due to
		 * quick movement of the device.
		 */

		/*
		 * TODO: add accelerations to shared memory map so host can
		 * access the information.
		 */

#ifdef CONFIG_CMD_LID_ANGLE
		if (accel_disp) {
			ts1 = get_time();
			ccprintf("%d,\t", acc_lid.x);
			ccprintf("%d,\t", acc_lid.y);
			ccprintf("%d,\t", acc_lid.z);
			ccprintf("%d,\t", acc_base.x);
			ccprintf("%d,\t", acc_base.y);
			ccprintf("%d,\t", acc_base.z);
			ccprintf("%d,\n", (int)(lid_angle_deg));
		}
#endif

		/* Delay appropriately to keep sampling time consistent. */
		ts1 = get_time();
		wait_us = accel_interval_ms*1000 - (ts1.val-ts0.val);
		if (wait_us > 0)
			task_wait_event(wait_us);
	}
}

#ifdef CONFIG_ACCEL_CALIBRATE
/**
 * Given three input vectors and three output vectors, solve for the rotation
 * matrix to get from the input vectors to the output vectors. Note, that this
 * operation is not guaranteed. In order to successfully calculate the rotation
 * matrix, the inputs must be linearly independent so that the matrix can be
 * inverted.
 *
 * This function solves the following matrix equation for R:
 * [ v_in1 ; v_in2 ; v_in3 ] * R = [v_out1 ; v_out2 ; v_out3 ]
 *
 * If input matrix is invertible the resulting rotation matrix is stored in R.
 */
static int solve_rotation_matrix(struct vector v_in1, struct vector v_in2,
				struct vector v_in3, struct vector v_out1,
				struct vector v_out2, struct vector v_out3,
				float (*R)[3][3])
{
	static float i[3][3];

	/* Calculate determinant of input matrix. */
	float det = v_in1.x*v_in2.y*v_in3.z + v_in2.x*v_in3.y*v_in1.z +
		v_in3.x*v_in1.y*v_in2.z - v_in1.x*v_in3.y*v_in2.z -
		v_in3.x*v_in2.y*v_in1.z - v_in2.x*v_in1.y*v_in3.z;

	/*
	 * If determinant is too close to zero, then input is not linearly
	 * independent enough, return an error. This threshold was
	 * experimentally determined.
	 */
	if (ABS(det) < 1e7)
		return EC_ERROR_UNKNOWN;

	/* Find inverse of input matrix. */
	i[0][0] = (v_in2.y*v_in3.z - v_in2.z*v_in3.y) / det;
	i[0][1] = (v_in1.z*v_in3.y - v_in1.y*v_in3.z) / det;
	i[0][2] = (v_in1.y*v_in2.z - v_in1.z*v_in2.y) / det;

	i[1][0] = (v_in2.z*v_in3.x - v_in2.x*v_in3.z) / det;
	i[1][1] = (v_in1.x*v_in3.z - v_in1.z*v_in3.x) / det;
	i[1][2] = (v_in1.z*v_in2.x - v_in1.x*v_in2.z) / det;

	i[2][0] = (v_in2.x*v_in3.y - v_in2.y*v_in3.x) / det;
	i[2][1] = (v_in1.y*v_in3.x - v_in1.x*v_in3.y) / det;
	i[2][2] = (v_in1.x*v_in2.y - v_in1.y*v_in2.x) / det;

	/* Multiple inverse of in matrix by out matrix and store into R. */
	(*R)[0][0] = i[0][0]*v_out1.x + i[0][1]*v_out2.x + i[0][2]*v_out3.x;
	(*R)[0][1] = i[0][0]*v_out1.y + i[0][1]*v_out2.y + i[0][2]*v_out3.y;
	(*R)[0][2] = i[0][0]*v_out1.z + i[0][1]*v_out2.z + i[0][2]*v_out3.z;

	(*R)[1][0] = i[1][0]*v_out1.x + i[1][1]*v_out2.x + i[1][2]*v_out3.x;
	(*R)[1][1] = i[1][0]*v_out1.y + i[1][1]*v_out2.y + i[1][2]*v_out3.y;
	(*R)[1][2] = i[1][0]*v_out1.z + i[1][1]*v_out2.z + i[1][2]*v_out3.z;

	(*R)[2][0] = i[2][0]*v_out1.x + i[2][1]*v_out2.x + i[2][2]*v_out3.x;
	(*R)[2][1] = i[2][0]*v_out1.y + i[2][1]*v_out2.y + i[2][2]*v_out3.y;
	(*R)[2][2] = i[2][0]*v_out1.z + i[2][1]*v_out2.z + i[2][2]*v_out3.z;

	ccprintf("\nUnits are in 100's. Divide by 100 to get real values.\n");
	ccprintf("%d\t%d\t%d\n%d\t%d\t%d\n%d\t%d\t%d\n",
	(int)((*R)[0][0]*100), (int)((*R)[0][1]*100), (int)((*R)[0][2]*100),
	(int)((*R)[1][0]*100), (int)((*R)[1][1]*100), (int)((*R)[1][2]*100),
	(int)((*R)[2][0]*100), (int)((*R)[2][1]*100), (int)((*R)[2][2]*100));

	return EC_SUCCESS;
}

/**
 * Calculate magnitude of a vector.
 */
static int vector_magnitude(const struct vector v)
{
	return sqrtf(SQ(v.x) + SQ(v.y) + SQ(v.z));
}
#endif /* CONFIG_ACCEL_CALIBRATE */

/*****************************************************************************/
/* Console commands */
#ifdef CONFIG_CMD_LID_ANGLE
static int command_ctrl_accels(int argc, char **argv)
{
	char *e;
	int val;

	if (argc > 3)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is on/off whether to display accel data. */
	if (argc > 1) {
		if (!parse_bool(argv[1], &val))
			return EC_ERROR_PARAM1;

		accel_disp = val;

		if (accel_disp)
			ccprintf("\nLidX\tLidY\tLidZ\tBaseX\tBaseY\tBaseZ\t"
				"Lid Angle\n");
	}

	/* Second arg changes the accel task time interval. */
	if (argc > 2) {
		val = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		accel_interval_ms = val;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accel, command_ctrl_accels,
	"on/off [interval]",
	"Control acceleration task.", NULL);
#endif /* CONFIG_CMD_LID_ANGLE */

#ifdef CONFIG_ACCEL_CALIBRATE
static int command_auto_calibrate(int argc, char **argv)
{
	char *e;
	int type, mag;
	int x = 0, y = 0, z = 0;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	type = strtoi(argv[1], &e, 0);

	if (*e || type < 0 || type > 1)
		return EC_ERROR_PARAM1;

	/*
	 * type of 0: calibrate the sensor orientation rotation matrix.
	 * type of 1: calibrate the up direction rotation matrix.
	 */
	if (type)
		ccprintf("Open lid to 90 degrees and rotate in space until all"
				" 3 directions are captured.\n");
	else
		ccprintf("Close lid and rotate in space until all 3 directions"
				" are captured.\n");

	/* Measure magnitude of base accelerometer. */
	mag = vector_magnitude(acc_base);

	while (1) {
		/*
		 * Only capture a sample if the magnitude of the acceleration
		 * is close to G, because this assures we won't calibrate with
		 * values biased by motion.
		 */
		if ((mag > ACCEL_G - AUTO_CAL_MAG_THRESHOLD) &&
			(mag < ACCEL_G + AUTO_CAL_MAG_THRESHOLD)) {

			/*
			 * Capture a sample when each axis exceeds some
			 * threshold. This guarantees linear independence.
			 */
			if (!x && ABS(acc_base.x) > AUTO_CAL_DIR_THRESHOLD) {
				rec_base1 = acc_base;
				rec_lid1 = type ? acc_lid : acc_lid_raw;

				ccprintf("Captured X\n");
				x = 1;
			}
			if (!y && ABS(acc_base.y) > AUTO_CAL_DIR_THRESHOLD) {
				rec_base2 = acc_base;
				rec_lid2 = type ? acc_lid : acc_lid_raw;

				ccprintf("Captured Y\n");
				y = 1;
			}
			if (!z && ABS(acc_base.z) > AUTO_CAL_DIR_THRESHOLD) {
				rec_base3 = acc_base;
				rec_lid3 = type ? acc_lid : acc_lid_raw;

				ccprintf("Captured Z\n");
				z = 1;
			}

			if (x && y && z)
				break;
		}

		/* Wait until next reading. */
		task_wait_event(accel_interval_ms*1000);
	}

	ccprintf("1: %d\t%d\t%d\t\t%d\t%d\t%d\n", rec_base1.x, rec_base1.y,
			rec_base1.z, rec_lid1.x, rec_lid1.y, rec_lid1.z);
	ccprintf("2: %d\t%d\t%d\t\t%d\t%d\t%d\n", rec_base2.x, rec_base2.y,
			rec_base2.z, rec_lid2.x, rec_lid2.y, rec_lid2.z);
	ccprintf("3: %d\t%d\t%d\t\t%d\t%d\t%d\n", rec_base3.x, rec_base3.y,
			rec_base3.z, rec_lid3.x, rec_lid3.y, rec_lid3.z);

	/* Solve for the rotation matrix and display final rotation matrix. */
	if (type == 0)
		solve_rotation_matrix(rec_lid1, rec_lid2, rec_lid3,
					rec_base1, rec_base2, rec_base3,
					&rot_sense_orientation);
	else
		solve_rotation_matrix(rec_base1, rec_base2, rec_base3,
					rec_lid1, rec_lid2, rec_lid3,
					&rot_up_direction);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelcalib, command_auto_calibrate,
	"0 - Calibrate sensor orientation rotation matrix, 1 - Calibrate up "
	"direction rotation matrix",
	"Auto calibrate the accelerometers", NULL);

static int command_print_rotation(int argc, char **argv)
{
	float (*R)[3][3];

	/* Print out both rotation matrices. */
	ccprintf("\nUnits are in 100's. Divide by 100 to get real values.\n");

	R = &rot_sense_orientation;
	ccprintf("Orientation R:\n%d\t%d\t%d\n%d\t%d\t%d\n%d\t%d\t%d\n\n",
	(int)((*R)[0][0]*100), (int)((*R)[0][1]*100), (int)((*R)[0][2]*100),
	(int)((*R)[1][0]*100), (int)((*R)[1][1]*100), (int)((*R)[1][2]*100),
	(int)((*R)[2][0]*100), (int)((*R)[2][1]*100), (int)((*R)[2][2]*100));

	R = &rot_up_direction;
	ccprintf("Up Direction R:\n%d\t%d\t%d\n%d\t%d\t%d\n%d\t%d\t%d\n",
	(int)((*R)[0][0]*100), (int)((*R)[0][1]*100), (int)((*R)[0][2]*100),
	(int)((*R)[1][0]*100), (int)((*R)[1][1]*100), (int)((*R)[1][2]*100),
	(int)((*R)[2][0]*100), (int)((*R)[2][1]*100), (int)((*R)[2][2]*100));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelprintrot, command_print_rotation,
	"",
	"Print both rotation matrices", NULL);
#endif /* CONFIG_ACCEL_CALIBRATE */
