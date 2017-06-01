/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests entropy source.
 */

#include "console.h"
#include "common.h"
#include "rollback.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

static int buckets[256];

/* Precomputed log2(x)*65536 table. */
static const int log2_mult = 65536;
static const int log2_table[256] = {
	0,   0,  65536, 103872, 131072, 152170, 169408, 183983, 196608, 207744,
	217706, 226717, 234944, 242512, 249519, 256042, 262144, 267876, 273280,
	278392, 283242, 287855, 292253, 296456, 300480, 304340, 308048, 311616,
	315055, 318373, 321578, 324678, 327680, 330589, 333412, 336153, 338816,
	341407, 343928, 346384, 348778, 351113, 353391, 355616, 357789, 359914,
	361992, 364026, 366016, 367966, 369876, 371748, 373584, 375385, 377152,
	378887, 380591, 382264, 383909, 385525, 387114, 388677, 390214, 391727,
	393216, 394682, 396125, 397547, 398948, 400328, 401689, 403030, 404352,
	405656, 406943, 408212, 409464, 410700, 411920, 413125, 414314, 415488,
	416649, 417795, 418927, 420046, 421152, 422245, 423325, 424394, 425450,
	426495, 427528, 428550, 429562, 430562, 431552, 432532, 433502, 434462,
	435412, 436353, 437284, 438206, 439120, 440025, 440921, 441809, 442688,
	443560, 444423, 445279, 446127, 446967, 447800, 448626, 449445, 450256,
	451061, 451859, 452650, 453435, 454213, 454985, 455750, 456510, 457263,
	458010, 458752, 459488, 460218, 460942, 461661, 462375, 463083, 463786,
	464484, 465177, 465864, 466547, 467225, 467898, 468566, 469229, 469888,
	470543, 471192, 471838, 472479, 473115, 473748, 474376, 475000, 475620,
	476236, 476848, 477456, 478060, 478661, 479257, 479850, 480439, 481024,
	481606, 482185, 482759, 483331, 483898, 484463, 485024, 485582, 486136,
	486688, 487236, 487781, 488323, 488861, 489397, 489930, 490459, 490986,
	491510, 492031, 492549, 493064, 493577, 494086, 494593, 495098, 495599,
	496098, 496594, 497088, 497579, 498068, 498554, 499038, 499519, 499998,
	500474, 500948, 501419, 501889, 502355, 502820, 503282, 503742, 504200,
	504656, 505109, 505561, 506010, 506457, 506902, 507345, 507786, 508224,
	508661, 509096, 509528, 509959, 510388, 510815, 511240, 511663, 512084,
	512503, 512921, 513336, 513750, 514162, 514572, 514981, 515387, 515792,
	516195, 516597, 516997, 517395, 517791, 518186, 518579, 518971, 519361,
	519749, 520136, 520521, 520904, 521286, 521667, 522046, 522423, 522799,
	523173, 523546, 523918
};

/* log base 2, multipled by 2**16 for precision. */
uint32_t log2(int32_t val)
{
	int32_t div = 0;

	if (val == 0)
		return 1 << 31;

	while (val > 255) {
		val = (val + 1) / 2;
		div++;
	}
	return (log2_mult * div) + log2_table[val];
}

void run_test(void)
{
	const int loopcount = 512;

	uint8_t buffer[32];
	timestamp_t t0, t1;
	int i, j;
	uint32_t entropy;
	const int totalcount = loopcount * sizeof(buffer);
	const int log2totalcount = log2(totalcount);

	memset(buckets, 0, sizeof(buckets));

	for (i = 0; i < loopcount; i++) {
		t0 = get_time();
		if (!board_get_entropy(buffer, sizeof(buffer))) {
			ccprintf("Cannot get entropy\n");
			test_fail();
			return;
		}
		t1 = get_time();
		if (i == 0)
			ccprintf("Got %d bytes in %ld us\n",
				sizeof(buffer), t1.val - t0.val);

		for (j = 0; j < sizeof(buffer); j++)
			buckets[buffer[j]]++;

		watchdog_reload();
	}

	ccprintf("Total count: %d\n", totalcount);
	ccprintf("Buckets: ");
	entropy = 0;
	for (j = 0; j < 256; j++) {
		/*
		 * Shannon entropy (base 2) is sum of -p[j] * log_2(p[j]).
		 * p[j] = buckets[j]/totalcount
		 * -p[j] * log_2(p[j])
		 *  = -(buckets[j]/totalcount) * log_2(buckets[j]/totalcount)
		 *  = buckets[j] * (log_2(totalcount) - log_2(buckets[j])
		 *                                               / totalcount
		 * Our log2() function is scaled by log2_mult for precision,
		 * so we need to divide by log2_mult at the end.
		 */
		entropy += buckets[j] * (log2totalcount - log2(buckets[j]))
			   / totalcount;
		ccprintf("%d;", buckets[j]);
		cflush();
	}
	ccprintf("\n");

	ccprintf("Entropy: %u/1000 bits\n", entropy * 1000 / log2_mult);

	/* We want at least 2 bits of entropy (out of a maximum of 8) */
	if ((entropy / log2_mult) >= 2)
		test_pass();
	else
		test_fail();
}
