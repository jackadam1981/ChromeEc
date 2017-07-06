/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MEC1322 SoC little FW
 *
 */

/* TODO MCHP Why naked?  This is dangerous except for
 * function/ISR wrappers using inline assembly.
 * lfw_main() makes many calls and has one local variable.
 * Naked C functions should not use local data unless the local
 * data can fit in CPU registers.
 * Note other C functions called by lfw_main() are not marked naked and
 * do include compiler generated prolog and epilog code.
 * We also do not know how much stack space is available when
 * EC_RO calls lfw_main().
 *
void lfw_main(void) __attribute__ ((noreturn, naked));
*/
void lfw_main(void) __attribute__ ((noreturn));
void fault_handler(void) __attribute__((naked));

struct int_vector_t {
	void   *stack_ptr;
	void   *reset_vector;
	void   *nmi;
	void   *hard_fault;
	void   *bus_fault;
	void   *usage_fault;
};

#define SPI_CHUNK_SIZE			1024
