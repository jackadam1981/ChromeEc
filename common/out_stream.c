/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "out_stream.h"

size_t out_stream_write(out_stream const * stream,
			uint8_t * buffer,
			size_t count)
{
	return stream->ops->write(stream, buffer, count);
}

void out_stream_flush(out_stream const * stream)
{
	stream->ops->flush(stream);
}

int out_stream_pause(out_stream const * stream)
{
	return stream->ops->pause(stream);
}

int out_stream_resume(out_stream const * stream)
{
	return stream->ops->resume(stream);
}

int out_stream_paused(out_stream const * stream)
{
	return stream->ops->paused(stream);
}

void out_stream_ready(out_stream const * stream)
{
	if (stream->ready)
		stream->ready(stream);
}
