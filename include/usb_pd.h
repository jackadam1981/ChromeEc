/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery module */

#ifndef __USB_PD_H
#define __USB_PD_H

#include "common.h"


/* --- Physical layer functions : chip specific --- */

/* Packet preparation/retrieval */

/**
 * Prepare packet reading state machine.
 *
 * @return opaque context for other reading functions.
 */
void *pd_init_dequeue(void);

/**
 * Prepare packet reading state machine.
 *
 * @param ctxt opaque context.
 * @param off  current position in the packet buffer.
 * @param len  minimum size to read in bits.
 * @param val  the read bits.
 * @return new position in the packet buffer.
 */
int pd_dequeue_bits(void *ctxt, int off, int len, uint32_t *val);

/**
 * Advance until the end of the preamble.
 *
 * @param ctxt opaque context.
 * @return new position in the packet buffer.
 */
int pd_find_preamble(void *ctxt);

/**
 * Write the preamble in the TX buffer.
 *
 * @param ctxt opaque context.
 * @return new position in the packet buffer.
 */
int pd_write_preamble(void *ctxt);

/**
 * Write one 10-period symbol in the TX packet.
 * corresponding to a quartet with 4b5b encoding
 * and Biphase Mark Coding.
 *
 * @param ctxt    opaque context.
 * @param bit_off current position in the packet buffer.
 * @param val10    the 10-bit integer.
 * @return new position in the packet buffer.
 */
int pd_write_sym(void *ctxt, int bit_off, uint32_t val10);

/**
 * Dump the current PD packet on the console for debug.
 *
 * @param ctxt opaque context.
 * @param msg  context string.
 */
void pd_dump_packet(void *ctxt, const char *msg);

/**
 * Change the TX data clock frequency.
 *
 * @param freq frequency in hertz.
 */
void pd_set_clock(int freq);

/* TX/RX callbacks */

/**
 * Start sending over the wire the prepared packet.
 *
 * @param ctxt    opaque context.
 * @param bit_len size of the packet in bits.
 */
void pd_start_tx(void *ctxt, int bit_len);
/* Call when we are done sending a packet */
void pd_tx_done(void);

/* Callback when the hardware has detected an incoming packet */
void pd_rx_event(void);
/* Start sampling the CC line for reception */
void pd_rx_start(void);
/* Call when we are done reading a packet */
void pd_rx_complete(void);

/* restart listening to the CC wire */
void pd_rx_enable_monitoring(void);
/* stop listening to the CC wire during transmissions */
void pd_rx_disable_monitoring(void);

/**
 * Initialize the hardware used for PD RX/TX.
 *
 * @return opaque context for other functions.
 */
void *pd_hw_init(void);

#endif  /* __USB_PD_H */
