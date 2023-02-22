/*
 * Copyright (c) 2022 Google, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ZEPHYR_CUSTOM_LOG_H__
#define __ZEPHYR_CUSTOM_LOG_H__

#ifndef static_assert
#define static_assert(...)
#endif

#include <pw_tokenizer/tokenize.h>
#include <pw_tokenizer/tokenize_to_global_handler.h>

#undef LOG_DBG
#undef LOG_INF
#undef LOG_WRN
#undef LOG_ERR

void SetMessage(const uint8_t* message, size_t size);

#define LOG_DBG(format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)
#define LOG_INF(format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)
#define LOG_WRN(format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)
#define LOG_ERR(format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)

//#undef ccputs
#undef cprintf
#undef cprints

//#define ccputs(format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)
#define cprintf(channel, format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)
#define cprints(channel, format, ...) PW_TOKENIZE_TO_CALLBACK(SetMessage, format, __VA_ARGS__)

#endif /* __ZEPHYR_CUSTOM_LOG_H__ */
