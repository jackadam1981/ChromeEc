#ifndef _CEC_H_
#define _CEC_H_

#include <stdbool.h>
#include <stdint.h>

void timer_handler(void);
int cec_send(uint8_t data[], int8_t byte_len);
int tv_on(int argc, char **argv);
int tv_off(int argc, char **argv);
int cec_init(void);

#endif // _CEC_H_
