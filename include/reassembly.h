#ifndef __EC_INCLUDE_REASSEMBLY_H
#define __EC_INCLUDE_REASSEMBLY_H

#include <queue.h>
#include <stddef.h>
#include <stdint.h>

/* The string REAS in ASCII in big endian. */
#define REASSEMBLY_MAGIC 0x52454153

struct reassembly_pdu {
	uint32_t magic;  /* Set to ASCII 'REAS' */
	uint32_t check;
	uint8_t  version;
	uint16_t size;
	uint8_t payload[0];
} __packed;

enum reassembly_result {
	RS_SUCCESS,
	RS_NEED_MORE_DATA,
	RS_TIMEOUT,
	RS_OVERFLOW,
	RS_CHECK_ERROR,
	RS_PROTOCOL_ERROR
};

struct reassembly_payload {
	void *rs_data;
	size_t rs_data_size;
};

size_t reassembly_overhead(void);
void *reassembly_register(void *buffer, size_t buffer_size,
			  int (*pdu_check_func)
			  (const void *check, size_t check_size,
			   const void *data, size_t data_size));
enum reassembly_result reassembly_feed(void *ctx, const struct queue *q, size_t coount);
void reassembly_get_payload(void *ctx, struct reassembly_payload *payload);

#endif  /* ! __EC_INCLUDE_REASSEMBLY_H */
