#ifndef __EC_INCLUDE_REASSEMBLY_H
#define __EC_INCLUDE_REASSEMBLY_H

#include <stddef.h>
#include <stdint.h>

enum reassembly_result {
	RS_SUCCESS,
	RS_NEED_MORE_DATA,
	RS_TIMEOUT,
	RS_OVERFLOW,
	RS_CHECK_ERROR
};

enum rs_check_type {
	RS_CHECK_SHA256,
	RS_CHECK_CRC32
};

struct reassembly_pdu {
	uint8_t  version;
	uint32_t check;
	uint16_t size;
	uint8_t payload[0];
} __packed;

size_t reassembly_overhead(void);
void *reassembly_register(void *buffer, size_t buffer_size,
			  int (*pdu_check_func)
			  (const void *check, size_t check_size,
			   const void *data, size_t data_size));
enum reassembly_result reassembly_feed(void *ctx, void *data, size_t size);


#endif  /* ! __EC_INCLUDE_REASSEMBLY_H */
