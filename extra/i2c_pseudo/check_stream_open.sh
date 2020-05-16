#!/bin/sh
#
# Check whether stream_open() symbol is available from the target kernel.
#
# Output meanings:
#   -1 : stream_open() is not available
#    0 : unknown if stream_open() is or is not available
#    1 : stream_open() is available

symbols="$(cat "/lib/modules/$(uname -r)/build/Module.symvers" | \
           awk '{print $2}' | grep -E '^(nonseekable_open|stream_open)$')"

if echo "$symbols" | grep -q '^stream_open$'; then
	echo 1
elif echo "$symbols" | grep -q '^nonseekable_open$'; then
	echo -1
else
	echo 0
fi
