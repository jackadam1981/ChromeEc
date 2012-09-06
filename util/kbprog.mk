# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CFLAGS = -g

kbprog: _kbprog.o
	$(CC) _kbprog.o -o kbprog

_kbprog.o: _kbprog.c kbprog.h symbols.h
	$(CC) $(CFLAGS) -c _kbprog.c

symbols.h: kbprog.c kbprog.py
	python kbprog.py < kbprog.c > symbols.h

_kbprog.c: kbprog.c kbprog.mk
	sed 's/@\(.[a-z_]*\)/\&\1_symbol/g' < kbprog.c > _kbprog.c

clean:
	rm *.o _kbprog.c kbprog
