/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file random.h
 * @brief Random number generators for the EC.
 *
 * This file contains implementation for generating random numbers for either a
 * uniform or normal distribution via the Box-Muller transformation. The code is
 * based on the work of John Burkardt which can be found at
 * https://people.sc.fsu.edu/~jburkardt/c_src/normal/normal.html
 *
 * References:
 *
 * Paul Bratley, Bennett Fox, Linus Schrage,
 *  A Guide to Simulation,
 *  Springer Verlag, pages 201-202, 1983.
 *
 *  Pierre L'Ecuyer,
 *  Random Number Generation,
 *  in Handbook of Simulation
 *  edited by Jerry Banks,
 *  Wiley Interscience, page 95, 1998.
 *
 *  Bennett Fox,
 *  Algorithm 647:
 *  Implementation and Relative Efficiency of Quasirandom
 *  Sequence Generators,
 *  ACM Transactions on Mathematical Software,
 *  Volume 12, Number 4, pages 362-376, 1986.
 *
 *  Peter Lewis, Allen Goodman, James Miller,
 *  A Pseudo-Random Number Generator for the System/360,
 *  IBM Systems Journal,
 *  Volume 8, pages 136-143, 1969.
 */

#ifndef __CROS_EC_RANDOM_H
#define __CROS_EC_RANDOM_H

/**
 * Generate a random number [0,1] using a uniform distribution.
 *
 * @param seed Pointer to the seed value.
 * @return A pseudo-random float between 0 and 1.
 */
float rnd_uniform(int *seed);

/**
 * Generate a random number using a normal distribution with a mean of 0 and a
 * standard deviation of 1.
 *
 * @param seed Pointer to the seed value.
 * @return A pseudo-random float with a mean of 0 and standard deviation of 1.
 */
float rnd_normal_01(int *seed);

/**
 * Generate a random number using a normal distribution and a given mean and
 * standard deviation.
 *
 * @param mean The mean to use.
 * @param stddev The standard deviation to use.
 * @param seed Pointer to the seed value.
 * @return A pseudo-random float with a given mean and standard deviation.
 */
float rnd_normal(float mean, float stddev, int *seed);

#endif /* __CROS_EC_RANDOM_H */
