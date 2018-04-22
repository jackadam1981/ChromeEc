#ifndef __EC_INCLUDE_REASSEMBLY_H
#define __EC_INCLUDE_REASSEMBLY_H

#include <queue.h>
#include <stddef.h>
#include <stdint.h>

struct reassembly_pdu {
	uint8_t  version;
	uint32_t check;
	uint16_t size;
	uint8_t payload[0];
} __packed;

enum reassembly_result {
	RS_SUCCESS,
	RS_NEED_MORE_DATA,
	RS_TIMEOUT,
	RS_OVERFLOW,
	RS_CHECK_ERROR
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
